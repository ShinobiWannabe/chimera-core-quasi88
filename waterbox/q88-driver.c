/* q88-driver.c - QUASI88 as a Chimera core.
 *
 * The driver is what the frontend calls: Init, FrameAdvance and the readers
 * of picture, sound and memory. Init turns the project's settings into a
 * QUASI88 command line and starts the machine through upstream's own
 * config_init and quasi88_start, as src/sysdepend/mini/main.c does, with
 * the project's disks in the drives. Each FrameAdvance writes what the
 * frontend says is held into the machine's keyboard ports and runs one
 * QUASI88 loop, which is one VSYNC of the PC-8801 (quasi88.c quasi88_loop).
 *
 * Built twice from the same source: once for the miniBox guest and once
 * natively (run-native.c), so the gate compares like with like. Both read
 * the "settings" and "slots" files of the folder they run in.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <emulibc.h>
#include <waterbox_settings.h>
#include <waterbox_slots.h>

#include "quasi88.h"
#include "initval.h"
#include "getconf.h"
#include "pc88main.h"
#include "pc88cpu.h"
#include "memory.h"
#include "keyboard.h"
#include "intr.h"
#include "drive.h"
#include "event.h"
#include "fdc.h"
#include "fname.h"
#include "screen.h"

#include "port/chimera-port.h"
#include "port/memwatch.h"
#include "port/trace.h"
#include "q88-driver.h"
#include "q88-input.h"

#define NELEMENTS(a) ((int)(sizeof(a) / sizeof((a)[0])))

static char g_loadError[512];
static int16_t *g_audio;
static int g_audioPairs;
static int g_inputRead;
static uint64_t g_frames;
/* the Start Time setting: seconds since 1970 in UTC */
static long long g_startTime;
static int g_started;
/* the floppy drives the controller worked on this frame, and last frame's,
 * which the lights show (see the drives section) */
static uint8_t g_accessed[2], g_lit[2];

/* the save-data window (see the save data section) */
#define SAVEDATA_WINDOW (256 * 1024)
static uint8_t *g_saveWindow;

/* ------------------------------------------------------------ the picture
 *
 * QUASI88's screen (port/graph.c) copied into the buffer Chimera reads, with
 * the alpha set. Turbo (SetRenderingEnabled) is the frontend's choice, not
 * the machine's: invisible, so a state taken in turbo does not bring it back. */
static uint32_t *g_frame;
static int g_width = 640, g_height = 400;
static ECL_INVISIBLE int g_render = 1;

static void copy_frame(void)
{
	int w, h, i;
	const uint32_t *src = chimera_screen(&w, &h);
	if (!src || !g_frame) return;
	if (w > Q88DRV_VIDEO_MAX_W) w = Q88DRV_VIDEO_MAX_W;
	if (h > Q88DRV_VIDEO_MAX_H) h = Q88DRV_VIDEO_MAX_H;
	g_width = w;
	g_height = h;
	for (i = 0; i < w * h; i++)
		g_frame[i] = src[i] | 0xff000000u;
}

/* ------------------------------------------------------------ the clock */

/* The calendar clock's time (port/clock.c): the Start Time and the emulated
 * time since power-on, a frame being 5/277 of a second. */
long long chimera_time_now(void)
{
	return g_startTime + (long long)(g_frames * 5 / 277);
}

/* ------------------------------------------------------------ settings */

static void setting_str(const char *key, const char *dflt, char *out, int outsz)
{
	if (wbx_setting_str(key, out, outsz) < 0) {
		strncpy(out, dflt, (size_t)outsz - 1);
		out[outsz - 1] = 0;
	}
}

static long clamp_long(long v, long lo, long hi) { return v < lo ? lo : v > hi ? hi : v; }

/* QUASI88's command line, built from the settings: upstream's own option
 * parser (getconf.c) then sets the machine up exactly as the same options
 * would on a desktop. */
#define MAX_ARGS 64
static char g_argText[MAX_ARGS][32];
static char *g_argv[MAX_ARGS + 1];
static int g_argc;

static void arg(const char *a)
{
	if (g_argc < MAX_ARGS) {
		strncpy(g_argText[g_argc], a, sizeof g_argText[0] - 1);
		g_argv[g_argc] = g_argText[g_argc];
		g_argc++;
	}
}

static void arg_long(const char *opt, long v)
{
	char n[24];
	snprintf(n, sizeof n, "%ld", v);
	arg(opt);
	arg(n);
}

static int g_kanji2, g_jisho, g_compatRom, g_rhythm, g_basicN;
/* the Boot From setting: what every reset boots from (machine_reset) */
static int g_bootRom;

static void build_command_line(void)
{
	char s[64];
	unsigned long dip;
	char *end;

	g_argc = 0;
	arg("quasi88");
	/* what the core always is: no configuration files, the screen alone at
	 * full size, every frame drawn, never a wait, never a pause */
	arg("-noconfig");
	arg("-nostatus");
	arg("-notool");
	arg("-full");
	arg_long("-frameskip", 1);
	arg("-noautoskip");
	arg("-nowait");
	arg("-nofocus");
	arg("-sound");
	arg_long("-samplefreq", CHIMERA_SOUND_RATE);
	arg("-nosamples");

	setting_str("basicMode", "V2", s, sizeof s);
	g_basicN = !strcmp(s, "N");
	if (g_basicN) arg("-n");
	else if (!strcmp(s, "V1S")) arg("-v1s");
	else if (!strcmp(s, "V1H")) arg("-v1h");
	else arg("-v2");
	setting_str("cpuClock", "4MHz", s, sizeof s);
	arg(!strcmp(s, "8MHz") ? "-8mhz" : "-4mhz");
	setting_str("soundBoard", "Sound Board I", s, sizeof s);
	arg(!strcmp(s, "Sound Board II") ? "-sd2" : "-sd");
	setting_str("soundGenerator", "MAME", s, sizeof s);
#if defined(USE_FMGEN)
	arg(!strcmp(s, "fmgen") ? "-fmgen" : "-nofmgen");
#else
	arg("-nofmgen");
#endif
	setting_str("cpuTiming", "0", s, sizeof s);
	arg_long("-cpu", clamp_long(strtol(s, NULL, 10), 0, 2));
	arg(wbx_setting_bool("fdcWait", 1) ? "-fdc_wait" : "-fdc_nowait");
	arg(wbx_setting_bool("memoryWait", 0) ? "-mem_wait" : "-mem_nowait");
	setting_str("monitor", "24kHz", s, sizeof s);
	arg(!strcmp(s, "15kHz") ? "-15k" : "-24k");
	setting_str("port", "Joystick", s, sizeof s);
	arg(!strcmp(s, "Mouse") ? "-mouse" : !strcmp(s, "None") ? "-nomouse" : "-joystick");
	setting_str("dipSwitches", "391A", s, sizeof s);
	dip = strtoul(s, &end, 16);
	if (end == s || *end || dip > 0xffff) dip = 0x391a;
	arg_long("-dipsw", (long)dip);
	setting_str("bootFrom", "Disk", s, sizeof s);
	g_bootRom = !strcmp(s, "ROM");
	arg(g_bootRom ? "-romboot" : "-diskboot");
	arg_long("-extram", clamp_long(wbx_setting_long("extraRam", 0), 0, 16));
	g_kanji2 = wbx_setting_bool("kanjiLevel2", 0) != 0;
	g_jisho = wbx_setting_bool("dictionaryRom", 0) != 0;
	g_rhythm = wbx_setting_bool("rhythmSamples", 0) != 0;
	arg(g_jisho ? "-jisho" : "-nojisho");
	arg_long("-fmvol", clamp_long(wbx_setting_long("volumeFM", 100), 0, 100));
	arg_long("-psgvol", clamp_long(wbx_setting_long("volumeSSG", 20), 0, 100));
	arg_long("-beepvol", clamp_long(wbx_setting_long("volumeBeep", 60), 0, 100));
	arg_long("-rhythmvol", clamp_long(wbx_setting_long("volumeRhythm", 100), 0, 200));
	arg_long("-adpcmvol", clamp_long(wbx_setting_long("volumeADPCM", 100), 0, 200));
	arg_long("-fmgenvol", clamp_long(wbx_setting_long("volumeFmgen", 100), 0, 100));
	/* the BASIC and disk ROMs from one combined file, M88's PC88.ROM
	 * (QUASI88's -compatrom), or from QUASI88's separate files */
	setting_str("romFiles", "PC88.ROM", s, sizeof s);
	g_compatRom = strcmp(s, "Separate files") != 0;
	if (g_compatRom) {
		arg("-compatrom");
		arg("pc88.rom");
	}
	g_argv[g_argc] = NULL;

	g_startTime = wbx_setting_long("startTime", 567993600);
}

/* ------------------------------------------------------------ disks
 *
 * The project's floppies arrive in the "floppy" slot, each mounted under its
 * own name. A D88 file may hold several disks, one after another, each
 * starting with its header, whose bytes 1Ch-1Fh give the disk's size. The
 * drives' media lists are every disk of every file, in order. */
#define MAX_MEDIA 256
static struct { char file[256]; int image; char name[280]; } g_media[MAX_MEDIA];
static int g_mediaCount;
/* the disk each drive's selector is on, and the disk in it (-1: none) */
static int g_selected[2], g_inserted[2] = { -1, -1 };
static int g_floppyFiles;

static int d88_images(const char *name)
{
	FILE *f = fopen(name, "rb");
	long at = 0, size;
	unsigned char h[0x20];
	int n = 0;
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	/* as upstream counts them (fdc.c disk_insert): at most MAX_NR_IMAGE,
	 * each wholly inside the file; a header's worth at least, so a size
	 * of zero cannot count one place forever */
	while (at + 0x20 <= size && n < MAX_NR_IMAGE) {
		unsigned long len;
		fseek(f, at, SEEK_SET);
		if (fread(h, 1, sizeof h, f) != sizeof h) break;
		len = h[0x1c] | (unsigned long)h[0x1d] << 8 | (unsigned long)h[0x1e] << 16 | (unsigned long)h[0x1f] << 24;
		if (len < 0x20 || at + (long)len > size) break;
		n++;
		at += (long)len;
	}
	fclose(f);
	return n ? n : 1;
}

static void list_media(void)
{
	int n = wbx_slot_count("floppy"), i, k;
	char name[256];
	g_mediaCount = 0;
	g_floppyFiles = 0;
	for (i = 0; i < n; i++) {
		int images;
		if (!wbx_slot_name("floppy", i, name, sizeof name) || !name[0]) continue;
		g_floppyFiles++;
		images = d88_images(name);
		for (k = 0; k < images && g_mediaCount < MAX_MEDIA; k++) {
			strcpy(g_media[g_mediaCount].file, name);
			g_media[g_mediaCount].image = k;
			if (images > 1) snprintf(g_media[g_mediaCount].name, sizeof g_media[0].name, "%s #%d", name, k + 1);
			else snprintf(g_media[g_mediaCount].name, sizeof g_media[0].name, "%s", name);
			g_mediaCount++;
		}
	}
}

/* QUASI88's start: the first file in drive 1 and the second in drive 2; a
 * single file holding two or more disks has its second disk in drive 2
 * (fname.c imagefile_all_open). */
static void choose_start_disks(void)
{
	int i;
	g_inserted[0] = g_inserted[1] = -1;
	if (g_mediaCount == 0) return;
	g_inserted[0] = 0;
	for (i = 1; i < g_mediaCount; i++)
		if (strcmp(g_media[i].file, g_media[0].file) != 0) break;
	if (i < g_mediaCount) g_inserted[1] = i;
	else if (g_mediaCount > 1) g_inserted[1] = 1;
	g_selected[0] = g_inserted[0];
	g_selected[1] = g_inserted[1] >= 0 ? g_inserted[1] : g_inserted[0];
}

static void drive_step(int drv, int delta)
{
	if (g_mediaCount < 1) return;
	g_selected[drv] = (g_selected[drv] + delta + g_mediaCount) % g_mediaCount;
}

/* Swap puts the selected disk in. When that disk is in the other drive, the
 * two drives trade disks, as the NP2kai core does (owner, 2026-11-14): one
 * disk cannot be in two drives. An empty drive gives the other nothing back:
 * that drive is left empty, and its selector stays where it was.
 * quasi88_disk_insert takes the old disk out first, so a disk that fails to
 * go in leaves the drive empty. */
static int drive_holds_disk(int drv) { return disk_image_exist(drv) && !drive_check_empty(drv); }

/* A disk QUASI88 takes but cannot show (an image number past the file's
 * last, fdc.c disk_insert) leaves the drive empty: so says the drive. */
static void drive_insert(int drv, int m)
{
	g_inserted[drv] = m >= 0 && quasi88_disk_insert(drv, g_media[m].file, g_media[m].image, FALSE) &&
		drive_holds_disk(drv) ? m : -1;
	if (g_inserted[drv] < 0) quasi88_disk_eject(drv);
}

static void drive_swap(int drv)
{
	int other = 1 - drv, m = g_selected[drv], cur = g_inserted[drv];
	if (m < 0 || m >= g_mediaCount || m == cur) return;
	drive_insert(drv, m);
	if (g_inserted[drv] == m && g_inserted[other] == m) {
		drive_insert(other, cur);
		if (cur >= 0) g_selected[other] = cur;
	}
}

static void drive_eject(int drv)
{
	if (g_inserted[drv] < 0) return;
	quasi88_disk_eject(drv);
	g_inserted[drv] = -1;
}

/* ------------------------------------------------------------ tapes
 *
 * The project's cassette tapes (T88 or CMT) arrive in the "tape" slot. The
 * first is in the data recorder at power-on; Previous and Next move a
 * selector, Swap puts the selected tape in (at its start), Eject takes it
 * out and Rewind winds it back. What the machine saves to tape goes into
 * TAPE_SAVE, made in memory (port/file-op.c), or appended to a tape of that
 * name the project gives. */
#define MAX_TAPES 64
#define TAPE_SAVE "tape-save.cmt"
static char g_tapes[MAX_TAPES][256];
static int g_tapeCount, g_tapeSelected, g_tapeInserted = -1;

static void list_tapes(void)
{
	int n = wbx_slot_count("tape"), i;
	g_tapeCount = 0;
	for (i = 0; i < n && g_tapeCount < MAX_TAPES; i++)
		if (wbx_slot_name("tape", i, g_tapes[g_tapeCount], sizeof g_tapes[0]) && g_tapes[g_tapeCount][0])
			g_tapeCount++;
	g_tapeSelected = 0;
	g_tapeInserted = g_tapeCount > 0 ? 0 : -1;
}

static void tape_step(int delta)
{
	if (g_tapeCount < 1) return;
	g_tapeSelected = (g_tapeSelected + delta + g_tapeCount) % g_tapeCount;
}

static void tape_swap(void)
{
	if (g_tapeSelected < 0 || g_tapeSelected >= g_tapeCount || g_tapeSelected == g_tapeInserted) return;
	g_tapeInserted = quasi88_load_tape_insert(g_tapes[g_tapeSelected]) ? g_tapeSelected : -1;
}

static void tape_eject(void)
{
	if (g_tapeInserted < 0) return;
	quasi88_load_tape_eject();
	g_tapeInserted = -1;
}

/* a tape that cannot be wound back is taken out (control.c) */
static void tape_rewind(void)
{
	if (g_tapeInserted < 0) return;
	if (!quasi88_load_tape_rewind()) g_tapeInserted = -1;
}

/* ------------------------------------------------------------ the ABI */

ECL_EXPORT const char *GetLoadError(void) { return g_loadError; }

/* the firmware QUASI88 cannot start without, by its mount names: the
 * separate ROM files, or the combined one */
static const char *const k_required[] = {
	"n88.rom", "n88_0.rom", "n88_1.rom", "n88_2.rom", "n88_3.rom", "n88n.rom", "disk.rom", "kanji1.rom" };
static const char *const k_requiredCompat[] = { "pc88.rom", "kanji1.rom" };

ECL_EXPORT int Init(void)
{
	int i;
	g_loadError[0] = 0;

	g_audio = (int16_t *)calloc(Q88DRV_AUDIO_MAX_PAIRS * 2, sizeof *g_audio);
	g_frame = (uint32_t *)calloc((size_t)Q88DRV_VIDEO_MAX_W * Q88DRV_VIDEO_MAX_H, sizeof *g_frame);
	if (!g_audio || !g_frame) {
		snprintf(g_loadError, sizeof g_loadError, "error.no_room_for_buffers");
		return 0;
	}
	g_saveWindow = (uint8_t *)alloc_invisible(SAVEDATA_WINDOW);
	chimera_trace_init();

	build_command_line();
	{
		const char *const *need = g_compatRom ? k_requiredCompat : k_required;
		int n = g_compatRom ? NELEMENTS(k_requiredCompat) : NELEMENTS(k_required);
		for (i = 0; i < n; i++)
			if (!chimera_file_exists(need[i])) {
				snprintf(g_loadError, sizeof g_loadError, "error.no_firmware\x1f%s", need[i]);
				return 0;
			}
	}
	/* The sizes QUASI88 can use: the firmware declarations take any size
	 * (Chimera refuses every size but a declared one), so the check is here.
	 * PC88.ROM is 88 KiB, or 112 KiB with the whole of N-BASIC (memory.c
	 * load_compat_rom); DISK.ROM is 2 KiB (2D unit) or 8 KiB. */
	{
		const char *name = g_compatRom ? "pc88.rom" : "disk.rom";
		int64_t len = chimera_file_length(name);
		int ok = g_compatRom ? (len == 0x16000 || len == 0x1c000) : (len == 0x800 || len == 0x2000);
		if (!ok) {
			snprintf(g_loadError, sizeof g_loadError, "error.bad_firmware_size\x1f%s\x1f%lld", name, (long long)len);
			return 0;
		}
		/* the N mode needs N-BASIC, which an 88 KiB PC88.ROM lacks: upstream
		 * would start it from a ROM of FFh, with no word of why */
		if (g_compatRom && g_basicN && len != 0x1c000) {
			snprintf(g_loadError, sizeof g_loadError, "error.no_nbasic");
			return 0;
		}
	}
	/* a ROM the settings leave out is not read even when it is mounted:
	 * with the combined ROM, the separate files are not part of the machine */
	if (g_compatRom) {
		static const char *const separate[] = { "n88.rom", "n88_0.rom", "n88_1.rom", "n88_2.rom", "n88_3.rom", "n88n.rom", "disk.rom" };
		for (i = 0; i < NELEMENTS(separate); i++) chimera_file_hide(separate[i]);
	} else {
		chimera_file_hide("pc88.rom");
	}
	if (!g_kanji2) chimera_file_hide("kanji2.rom");
	if (!g_jisho) chimera_file_hide("n88jisho.rom");
	if (!g_rhythm) {
		static const char *const rhythm[] = { "ym2608_adpcm_rom.bin", "2608_bd.wav", "2608_sd.wav",
			"2608_top.wav", "2608_hh.wav", "2608_tom.wav", "2608_rim.wav" };
		for (i = 0; i < NELEMENTS(rhythm); i++) chimera_file_hide(rhythm[i]);
	}

	if (!config_init(g_argc, g_argv, NULL, NULL, NULL)) {
		snprintf(g_loadError, sizeof g_loadError, "error.start_failed");
		return 0;
	}
	list_media();
	choose_start_disks();
	for (i = 0; i < 2; i++) {
		free(config_image.d[i]);
		config_image.d[i] = NULL;
		config_image.n[i] = 0;
		config_image.ro[i] = FALSE;
		if (g_inserted[i] >= 0) {
			config_image.d[i] = (char *)malloc(strlen(g_media[g_inserted[i]].file) + 1);
			if (config_image.d[i]) strcpy(config_image.d[i], g_media[g_inserted[i]].file);
			config_image.n[i] = g_media[g_inserted[i]].image;
		}
	}
	list_tapes();
	free(config_image.t[CLOAD]);
	free(config_image.t[CSAVE]);
	config_image.t[CLOAD] = NULL;
	config_image.t[CSAVE] = (char *)malloc(sizeof TAPE_SAVE);
	if (config_image.t[CSAVE]) strcpy(config_image.t[CSAVE], TAPE_SAVE);
	if (g_tapeInserted >= 0) {
		config_image.t[CLOAD] = (char *)malloc(strlen(g_tapes[g_tapeInserted]) + 1);
		if (config_image.t[CLOAD]) strcpy(config_image.t[CLOAD], g_tapes[g_tapeInserted]);
	}
	quasi88_start();
	/* a disk or tape QUASI88 could not open is not in its drive */
	for (i = 0; i < 2; i++)
		if (g_inserted[i] >= 0 && !drive_holds_disk(i)) g_inserted[i] = -1;
	if (g_tapeInserted >= 0 && !file_tape[CLOAD][0]) g_tapeInserted = -1;
	g_started = 1;
	return 1;
}

/* ------------------------------------------------------------ input
 *
 * Chimera sends levels: SetButton and SetAxis before each FrameAdvance say
 * what is held and how far the mouse moved. The keyboard ports are written
 * whole each frame from what is held, as the keyboard's matrix would read
 * (q88-input.h: the bits each key pulls low); the locking keys, KANA and
 * CAPS, change on a press. The levels, the last frame's levels and the
 * locks are machine state (ordinary globals), so savestates and movies
 * carry them. */
enum { KEY, LOCK, JOY, FDD1_PREV, FDD1_NEXT, FDD1_SWAP, FDD1_EJECT, FDD2_PREV, FDD2_NEXT, FDD2_SWAP, FDD2_EJECT, RESET,
	TAPE_PREV, TAPE_NEXT, TAPE_SWAP, TAPE_EJECT, TAPE_REWIND };
static const uint8_t k_keyMasks[][15] = Q88_KEY_MASKS;

static const struct { int kind, a, b; } k_buttons[] = {
#define X(name, kind, a, b) { kind, a, b },
	Q88_BUTTONS(X)
#undef X
};
#define BUTTON_COUNT NELEMENTS(k_buttons)

static uint8_t g_level[BUTTON_COUNT];
static uint8_t g_last[BUTTON_COUNT];
static int32_t g_axis[Q88_AXIS_COUNT];
/* the locking keys' locks: port 08h bit 5 (KANA), port 0Ah bit 7 (CAPS) */
static uint8_t g_locks[15];

/* the engine's input-poll callback: a slot address, the same in every
 * session, so it is invisible to savestates (porting-a-core.md) */
static ECL_INVISIBLE uint64_t g_pollCallback;

ECL_EXPORT void SetInputPollCallback(uint64_t fn) { g_pollCallback = fn; }

void chimera_input_read(void)
{
	g_inputRead = 1;
	if (g_pollCallback)
		((uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))
		 (uintptr_t)g_pollCallback)(0, 0, 0, 0, 0, 0);
}

ECL_EXPORT void SetButton(int32_t index, int32_t state)
{
	if (index >= 0 && index < BUTTON_COUNT)
		g_level[index] = state ? 1 : 0;
}

ECL_EXPORT void SetAxis(int32_t index, int32_t value)
{
	if (index >= 0 && index < Q88_AXIS_COUNT)
		g_axis[index] = value;
}

static int rose(int i) { return g_level[i] && !g_last[i]; }

static void machine_reset(void)
{
	T_RESET_CFG cfg;
	quasi88_get_reset_cfg(&cfg);
	/* Upstream's quasi88_disk_insert sets boot_from_rom to FALSE whenever a
	 * disk goes into drive 1 (control.c), so a swap would turn the next
	 * reset into a disk boot. A PC-8801 boots from what its switch says:
	 * the Boot From setting. */
	cfg.boot_from_rom = g_bootRom;
	quasi88_reset(&cfg);
}

static void apply_input(void)
{
	uint8_t pulled[16];
	int i, p;

	memset(pulled, 0, sizeof pulled);
	for (i = 0; i < BUTTON_COUNT; i++) {
		switch (k_buttons[i].kind) {
		case KEY:
			if (g_level[i])
				for (p = 0; p < 15; p++) pulled[p] |= k_keyMasks[k_buttons[i].a][p];
			break;
		case LOCK:
			if (rose(i)) g_locks[k_buttons[i].a] ^= (uint8_t)k_buttons[i].b;
			break;
		case JOY:
			if (g_level[i]) pulled[0x0f] |= (uint8_t)(1 << k_buttons[i].a);
			break;
		case FDD1_PREV: if (rose(i)) drive_step(0, -1); break;
		case FDD1_NEXT: if (rose(i)) drive_step(0, +1); break;
		case FDD1_SWAP: if (rose(i)) drive_swap(0); break;
		case FDD1_EJECT: if (rose(i)) drive_eject(0); break;
		case FDD2_PREV: if (rose(i)) drive_step(1, -1); break;
		case FDD2_NEXT: if (rose(i)) drive_step(1, +1); break;
		case FDD2_SWAP: if (rose(i)) drive_swap(1); break;
		case FDD2_EJECT: if (rose(i)) drive_eject(1); break;
		case RESET: if (rose(i)) machine_reset(); break;
		case TAPE_PREV: if (rose(i)) tape_step(-1); break;
		case TAPE_NEXT: if (rose(i)) tape_step(+1); break;
		case TAPE_SWAP: if (rose(i)) tape_swap(); break;
		case TAPE_EJECT: if (rose(i)) tape_eject(); break;
		case TAPE_REWIND: if (rose(i)) tape_rewind(); break;
		}
	}
	/* QUASI88's key_scan: 00h-0Eh the keyboard ports, 0Fh the controller
	 * port's bits, 10h-11h its own work for the shifted keys (left all up) */
	for (p = 0; p < 15; p++) key_scan[p] = (uint8_t)~(pulled[p] | g_locks[p]);
	key_scan[0x0f] = (uint8_t)~pulled[0x0f];
	key_scan[0x10] = key_scan[0x11] = 0xff;
	if (g_axis[Q88_AXIS_MOUSE_X] || g_axis[Q88_AXIS_MOUSE_Y])
		keyboard_mouse_move(g_axis[Q88_AXIS_MOUSE_X], g_axis[Q88_AXIS_MOUSE_Y], TRUE);
	memcpy(g_last, g_level, sizeof g_last);
	/* the mouse's movement is spent once applied; the next frame says again */
	g_axis[Q88_AXIS_MOUSE_X] = g_axis[Q88_AXIS_MOUSE_Y] = 0;
}

ECL_EXPORT void FrameAdvance(uint64_t packed)
{
	int r, guard;
	/* More than 64 buttons, so every one comes through SetButton (the
	 * wide-input channel) and the packed word is not used. */
	(void)packed;
	if (!g_started) return;
	g_inputRead = 0;
	apply_input();
	/* one VSYNC: quasi88_loop returns QUASI88_LOOP_ONE when a frame is done,
	 * QUASI88_LOOP_BUSY when it only changed mode */
	for (guard = 0; guard < 16; guard++) {
		r = quasi88_loop();
		if (r != QUASI88_LOOP_BUSY) break;
	}
	g_audioPairs = chimera_sound_take(g_audio, Q88DRV_AUDIO_MAX_PAIRS);
	memcpy(g_lit, g_accessed, sizeof g_lit);
	memset(g_accessed, 0, sizeof g_accessed);
	if (g_render) copy_frame();
	g_frames++;
}

/* Turbo: skip only what is pure output, the copy into the BGRA buffer.
 * QUASI88 still draws into its own screen every frame, because it draws only
 * what changed since the last frame drawn. */
ECL_EXPORT void SetRenderingEnabled(int32_t on) { g_render = on != 0; }

ECL_EXPORT int32_t GetVsyncNumerator(void) { return 277; }
ECL_EXPORT int32_t GetVsyncDenominator(void) { return 5; }

ECL_EXPORT uint32_t *GetVideoBgra(void) { return g_frame; }
ECL_EXPORT int GetVideoWidth(void) { return g_width; }
ECL_EXPORT int GetVideoHeight(void) { return g_height; }

ECL_EXPORT int16_t *GetAudio(void) { return g_audio; }
ECL_EXPORT int GetAudioSampleCount(void) { return g_audioPairs; }

ECL_EXPORT int InputWasRead(void) { return g_inputRead; }

/* ------------------------------------------------------------ memory
 *
 * The main CPU's RAM first, as tools open on the first; then the 4 KB of
 * fast RAM behind F000h-FFFFh (where the text screen usually is), the
 * graphics VRAM (planes B, R, G and QUASI88's spare byte, interleaved: 16 K
 * addresses of 4 bytes), the disk unit CPU's RAM (4000h-7FFFh of its
 * space), the extended RAM cards, and the two CPUs' ROMs, read only. */
static uint8_t *dom_ptr(int i, int64_t *size, int *writable)
{
	*writable = 1;
	switch (i) {
	case 0: *size = 0x10000; return main_ram;
	case 1: *size = 0x1000; return main_high_ram;
	case 2: *size = 0x4000 * 4; return (uint8_t *)main_vram;
	case 3: *size = 0x4000; return sub_romram + 0x4000;
	case 4: *size = ext_ram && use_extram ? (int64_t)use_extram * 4 * 0x8000 : 0; return ext_ram ? (uint8_t *)ext_ram : NULL;
	case 5: *writable = 0; *size = 0x8000; return main_rom;
	case 6: *writable = 0; *size = 0x2000 * 4; return (uint8_t *)main_rom_ext;
	case 7: *writable = 0; *size = 0x8000; return main_rom_n;
	case 8: *writable = 0; *size = 0x2000; return sub_romram;
	}
	*size = 0;
	return NULL;
}

static const char *const k_domains[] = {
	"Main RAM", "High RAM", "VRAM", "Sub CPU RAM", "Extended RAM",
	"N88 ROM", "N88 Extended ROM", "N ROM", "Sub CPU ROM" };
#define DOMAIN_COUNT NELEMENTS(k_domains)

ECL_EXPORT int GetMemoryDomainCount(void) { return DOMAIN_COUNT; }
ECL_EXPORT const char *GetMemoryDomainName(int i) { return i >= 0 && i < DOMAIN_COUNT ? k_domains[i] : NULL; }
ECL_EXPORT uint8_t *GetMemoryDomainPtr(int i) { int64_t s; int w; return dom_ptr(i, &s, &w); }
ECL_EXPORT int64_t GetMemoryDomainSize(int i) { int64_t s; int w; dom_ptr(i, &s, &w); return s; }
ECL_EXPORT int GetMemoryDomainWritable(int i) { int64_t s; int w; dom_ptr(i, &s, &w); return i >= 0 && i < DOMAIN_COUNT && w; }
/* With no extended RAM card its domain has no bytes: kept in the list, so
 * every domain keeps its number, but left out of the memory tools. */
ECL_EXPORT int32_t GetMemoryDomainHidden(int32_t i) { return GetMemoryDomainSize(i) == 0; }

/* ------------------------------------------------------------ calls at memory
 *
 * Chimera issues 496, 534 and 571: a Lua script's event.on_bus_exec,
 * on_bus_read and on_bus_write stop the machine at an address of either
 * Z80, part way through a frame, and a read or write may be given another
 * value. port/memwatch.c keeps what is watched and makes the calls;
 * everything it keeps is ECL_INVISIBLE, so no savestate carries it. Only
 * the watch build (CHIMERA_MEMWATCH) has them: the setting Lua Memory Events
 * picks it (q88-entry.c). */
#if defined(CHIMERA_MEMWATCH)
ECL_EXPORT const char *GetMemoryCallbackScopes(void) { return chimera_watch_scopes(); }
/* reads (1 << 1) and writes (1 << 2) may be changed; an instruction may not */
ECL_EXPORT int32_t GetMemoryCallbackReplaceable(void) { return (1 << 1) | (1 << 2); }
ECL_EXPORT const uint64_t *GetMemoryCallbackPc(void) { return chimera_watch_pc(); }
ECL_EXPORT void SetMemoryCallback(uint64_t fn) { chimera_watch_set_callback(fn); }
ECL_EXPORT int32_t AddMemoryWatch(int32_t kind, int32_t scope, uint64_t address)
{
	return chimera_watch_add(kind, scope, address);
}
ECL_EXPORT void RemoveMemoryWatch(int32_t kind, int32_t scope, uint64_t address)
{
	chimera_watch_remove(kind, scope, address);
}
#endif

/* ------------------------------------------------------------ drives
 *
 * The two floppy drives, when the project has a disk. A drive's light is on
 * for a frame in which the disk controller worked on it at any moment (the
 * hook patches/0002 adds to fdc.c fdc_ctrl marks it; the frame's end latches
 * it), so a short read is never missed (owner, 2026-11-14). Every disk of
 * the project can go in either drive: Previous and Next move the selector
 * and Swap puts the selected disk in (apply_input).
 * The floppy drives come first, when the project has a disk, then the data
 * recorder, when it has a tape: its light is the cassette motor's relay
 * (port 30h bit 3) with a tape in. All of it is guest memory, so a rewind
 * takes it back. */

void chimera_drive_access(int drv)
{
	if (drv >= 0 && drv < 2) g_accessed[drv] = 1;
}

static int floppy_drives(void) { return g_mediaCount > 0 ? 2 : 0; }
static int is_tape(int32_t i) { return g_tapeCount > 0 && i == floppy_drives(); }

ECL_EXPORT int32_t GetDriveCount(void) { return floppy_drives() + (g_tapeCount > 0); }
ECL_EXPORT const char *GetDriveName(int32_t i)
{
	if (is_tape(i)) return "Tape";
	return i >= 0 && i < floppy_drives() ? (i == 0 ? "Drive 1" : "Drive 2") : NULL;
}
ECL_EXPORT int32_t GetDriveLight(int32_t i)
{
	if (is_tape(i)) return g_tapeInserted >= 0 && (sys_ctrl & 0x08) != 0;
	return i >= 0 && i < floppy_drives() && g_lit[i];
}
ECL_EXPORT int32_t GetDriveMediaCount(int32_t i)
{
	if (is_tape(i)) return g_tapeCount;
	return i >= 0 && i < floppy_drives() ? g_mediaCount : 0;
}
ECL_EXPORT const char *GetDriveMediaName(int32_t i, int32_t n)
{
	if (is_tape(i)) return n >= 0 && n < g_tapeCount ? g_tapes[n] : NULL;
	return i >= 0 && i < floppy_drives() && n >= 0 && n < g_mediaCount ? g_media[n].name : NULL;
}
ECL_EXPORT int32_t GetDriveMediaSelected(int32_t i)
{
	if (is_tape(i)) return g_tapeSelected;
	return i >= 0 && i < floppy_drives() ? g_selected[i] : 0;
}
ECL_EXPORT int32_t GetDriveMediaInserted(int32_t i)
{
	if (is_tape(i)) return g_tapeInserted;
	return i >= 0 && i < floppy_drives() ? g_inserted[i] : -1;
}

/* ------------------------------------------------------------ registers
 *
 * Both Z80s' registers, read only: the main CPU's, then the disk unit's,
 * named with "Sub " in front. */
static const char *const k_regNames[] = {
	"AF", "BC", "DE", "HL", "IX", "IY", "PC", "SP", "AF'", "BC'", "DE'", "HL'", "I", "R", "IFF", "IM",
	"Sub AF", "Sub BC", "Sub DE", "Sub HL", "Sub IX", "Sub IY", "Sub PC", "Sub SP",
	"Sub AF'", "Sub BC'", "Sub DE'", "Sub HL'", "Sub I", "Sub R", "Sub IFF", "Sub IM" };
#define REG_COUNT NELEMENTS(k_regNames)

ECL_EXPORT int32_t GetRegisterCount(void) { return REG_COUNT; }
ECL_EXPORT const char *GetRegisterName(int32_t i) { return i >= 0 && i < REG_COUNT ? k_regNames[i] : NULL; }
ECL_EXPORT int32_t GetRegisterBits(int32_t i) { return i < 0 || i >= REG_COUNT ? 0 : (i % 16) >= 12 ? 8 : 16; }
ECL_EXPORT const char *GetRegisterRoles(void) { return "pc PC\nsp SP\n"; }

ECL_EXPORT int64_t GetRegisterValue(int32_t i)
{
	const z80arch *z = i < 16 ? &z80main_cpu : &z80sub_cpu;
	switch (i % 16) {
	case 0: return z->AF.W;   case 1: return z->BC.W;   case 2: return z->DE.W;   case 3: return z->HL.W;
	case 4: return z->IX.W;   case 5: return z->IY.W;   case 6: return z->PC.W;   case 7: return z->SP.W;
	case 8: return z->AF1.W;  case 9: return z->BC1.W;  case 10: return z->DE1.W; case 11: return z->HL1.W;
	case 12: return z->I;
	case 13: return (z->R & 0x7f) | (z->R_saved & 0x80);
	case 14: return z->IFF;
	case 15: return z->IM;
	}
	return 0;
}

/* -------------------------------------------- save data (docs/save-data.md)
 *
 * What a PC-8801 keeps is its disks and tapes: every disk file the machine
 * has written to leaves under the name it was mounted with, the whole file,
 * so it goes back into the project as a disk; so does tape-save.cmt, what
 * it saved to tape. The bytes go out through the window, a
 * piece at a time; the window is ECL_INVISIBLE memory, a view the host reads
 * between frames, not machine state. */
static int g_saveList[512];
static int g_saveCount;

ECL_EXPORT int32_t GetSaveDataFileCount(void)
{
	int i, n = chimera_file_count();
	g_saveCount = 0;
	for (i = 0; i < n && g_saveCount < NELEMENTS(g_saveList); i++)
		if (chimera_file_written(i)) g_saveList[g_saveCount++] = i;
	return g_saveCount;
}

ECL_EXPORT const char *GetSaveDataFileName(int32_t index)
{
	return index >= 0 && index < g_saveCount ? chimera_file_name(g_saveList[index]) : NULL;
}

ECL_EXPORT int64_t GetSaveDataFileSize(int32_t index)
{
	return index >= 0 && index < g_saveCount ? chimera_file_size(g_saveList[index]) : 0;
}

ECL_EXPORT const uint8_t *GetSaveDataFileBuffer(int32_t index)
{
	(void)index;
	return NULL;	/* use the window */
}

ECL_EXPORT const uint8_t *GetSaveDataScratch(void) { return g_saveWindow; }

ECL_EXPORT int64_t ReadSaveDataFile(int32_t index, int64_t offset, int64_t len)
{
	int64_t size;
	if (!g_saveWindow || index < 0 || index >= g_saveCount || len <= 0 || offset < 0) return 0;
	size = chimera_file_size(g_saveList[index]);
	if (offset >= size) return 0;
	if (len > SAVEDATA_WINDOW) len = SAVEDATA_WINDOW;
	if (len > size - offset) len = size - offset;
	memcpy(g_saveWindow, chimera_file_data(g_saveList[index]) + offset, (size_t)len);
	return len;
}

/* ------------------------------------------------------------ the gate */

/* A digest of the visible picture, one a frame. */
ECL_EXPORT uint64_t GetFrameDigest(void)
{
	uint64_t h = 1469598103934665603ULL;
	int i;
	for (i = 0; g_frame && i < g_width * g_height; i++) { h ^= g_frame[i]; h *= 1099511628211ULL; }
	h ^= (uint64_t)g_width * 65537u + (uint64_t)g_height;
	h *= 1099511628211ULL;
	return h;
}

/* And of the frame's sound, so the gate compares what is heard too. */
ECL_EXPORT uint64_t GetAudioDigest(void)
{
	uint64_t h = 1469598103934665603ULL;
	int i;
	for (i = 0; i < g_audioPairs * 2; i++) { h ^= (uint16_t)g_audio[i]; h *= 1099511628211ULL; }
	return h;
}

/* The main CPU's states since power-on: its VSYNCs so far, each
 * state_of_vsync states, and the states since the last one. Counted from
 * upstream's own VSYNC counter (intr.c), not from FrameAdvance: with CPU
 * Timing 0 a frame can end on the disk unit's time while the main CPU
 * stands still (pc88sub.c sub_INT_update), and that frame adds nothing. */
ECL_EXPORT uint64_t GetEmulatedClocks(void)
{
	return (uint64_t)(unsigned)quasi88_info_vsync_count() * (uint64_t)state_of_vsync +
	       (uint64_t)(int64_t)(state_of_cpu + z80main_cpu.state0);
}

ECL_EXPORT uint64_t GetFrameCount(void) { return g_frames; }
