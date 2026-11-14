/* audio.c - the sound output QUASI88's MAME sound layer calls
 * (src/snddrv/xmame/quasi88/mame-quasi88.h), for the Chimera core. Modelled
 * on upstream's src/sysdepend/mini/audio.c.
 *
 * The MAME layer mixes one frame at a time and hands it to
 * osd_update_audio_stream; here it is kept for the driver, which takes it
 * at the end of the frame (chimera_sound_take). The rate is 44320 Hz, so a
 * frame at QUASI88's 55.4 Hz is exactly 800 samples (chimera-port.h).
 *
 * The sound options are upstream's own table (mini/audio.c's), which the
 * driver's command line sets. */
#include <string.h>

#include "quasi88.h"
#include "device.h"
#include "snddrv.h"

#ifdef USE_SOUND

#include "mame-quasi88.h"

static int16_t g_frame[CHIMERA_SOUND_FRAME * 2];
static int g_pairs;

int chimera_sound_take(int16_t *out, int maxPairs)
{
	int n = g_pairs < maxPairs ? g_pairs : maxPairs;
	memcpy(out, g_frame, (size_t)n * 2 * sizeof *out);
	g_pairs = 0;
	return n;
}

int xmame_config_init(void) { return 0; }
void xmame_config_exit(void) {}

static const T_CONFIG_TABLE xmame_options[] = {
	{ 351, "sound",      X_FIX, &use_sound,           TRUE,  0,     0, OPT_SAVE },
	{ 351, "nosound",    X_FIX, &use_sound,           FALSE, 0,     0, OPT_SAVE },
	{ 353, "fmgen",      X_FIX, &use_fmgen,           TRUE,  0,     0, OPT_SAVE },
	{ 353, "nofmgen",    X_FIX, &use_fmgen,           FALSE, 0,     0, OPT_SAVE },
	{ 355, "fmvol",      X_INT, &fmvol,               0,     100,   0, OPT_SAVE },
	{ 356, "psgvol",     X_INT, &psgvol,              0,     100,   0, OPT_SAVE },
	{ 357, "beepvol",    X_INT, &beepvol,             0,     100,   0, OPT_SAVE },
	{ 358, "pcgvol",     X_INT, &pcgvol,              0,     100,   0, OPT_SAVE },
	{ 359, "rhythmvol",  X_INT, &rhythmvol,           0,     200,   0, OPT_SAVE },
	{ 360, "adpcmvol",   X_INT, &adpcmvol,            0,     200,   0, OPT_SAVE },
	{ 361, "fmgenvol",   X_INT, &fmgenvol,            0,     100,   0, OPT_SAVE },
	{ 362, "samplevol",  X_INT, &samplevol,           0,     100,   0, OPT_SAVE },
	{ 363, "samplefreq", X_INT, &options.samplerate,  8000,  48000, 0, OPT_SAVE },
	{ 364, "samples",    X_FIX, &options.use_samples, 1,     0,     0, OPT_SAVE },
	{ 364, "nosamples",  X_FIX, &options.use_samples, 0,     0,     0, OPT_SAVE },
	{   0, NULL,         X_INV, 0,                    0,     0,     0, 0        },
};

const T_CONFIG_TABLE *xmame_config_get_opt_tbl(void) { return xmame_options; }

void xmame_config_show_option(FILE *fp) { (void)fp; }

int xmame_config_check_option(char *opt1, char *opt2, int priority)
{
	(void)opt1; (void)opt2; (void)priority;
	return 0;
}

int xmame_config_save_option(void (*real_write)(const char *opt_name, const char *opt_arg))
{
	(void)real_write;
	return 0;
}

T_SNDDRV_CONFIG *xmame_config_get_sndopt_tbl(void) { return NULL; }

int xmame_has_audiodevice(void) { return FALSE; }
int xmame_has_mastervolume(void) { return FALSE; }

int osd_start_audio_stream(int stereo)
{
	(void)stereo;
	g_pairs = 0;
	return CHIMERA_SOUND_FRAME;
}

int osd_update_audio_stream(INT16 *buffer)
{
	memcpy(g_frame, buffer, sizeof g_frame);
	g_pairs = CHIMERA_SOUND_FRAME;
	return CHIMERA_SOUND_FRAME;
}

void osd_stop_audio_stream(void) {}
void osd_update_video_and_audio(void) {}
void osd_sound_enable(int enable) { (void)enable; }
void osd_set_mastervolume(int attenuation) { (void)attenuation; }
int osd_get_mastervolume(void) { return VOL_MIN; }

#else

int chimera_sound_take(int16_t *out, int maxPairs)
{
	(void)out; (void)maxPairs;
	return 0;
}

#endif /* USE_SOUND */
