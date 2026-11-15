/* run-native.c - the native reference harness.
 *
 * Runs the driver exactly as the frontend would, without the sandbox: in a
 * folder holding the project's mounted files (the firmware under its ids,
 * the disks, and "settings" and "slots" when the run needs them), for a
 * number of frames, and prints a line a frame with the picture digest,
 * picture size, sound count, sound digest, clock, whether the controls were
 * read, and the drive lights.
 *
 * Usage: run-native <folder> <frames> [out.ppm] [mem.bin]
 *   Q88_PRESS, Q88_MOUSE  input, as press-script.h describes
 *   Q88_SAVEDATA=dir   write the save-data export there after the run
 *   Q88_STALL_FRAME=n  stall the host 300 ms before frame n: the machine must
 *                      not notice
 *   Q88_REGS=1         print the CPUs' registers after the run, as REGS
 *   Q88_WAV=file       write the whole run's sound there, as a 44320 Hz
 *                      16-bit stereo .wav
 *   out.ppm  the last frame's picture
 *   mem.bin  the main memory domain after the last frame
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "q88-driver.h"
#include "press-script.h"
#include "watch-script.h"

/* The guest's allocators, as plain memory. */
void *alloc_invisible(size_t size) { return calloc(1, size); }
void *alloc_sealed(size_t size) { return calloc(1, size); }
void *alloc_plain(size_t size) { return calloc(1, size); }
void _debug_puts(const char *s) { fputs(s, stderr); }

/* Q88_WATCH (watch-script.h): the engine's side of the calls at memory */
static uintptr_t __attribute__((unused)) watch_called(uintptr_t kind, uintptr_t scope, uintptr_t address, uintptr_t value,
	uintptr_t size, uintptr_t replacement)
{
	return watch_note(kind, scope, address, value, size, replacement);
}

static void write_ppm(const char *path)
{
	FILE *f = fopen(path, "wb");
	const uint32_t *p = GetVideoBgra();
	int w = GetVideoWidth(), h = GetVideoHeight(), i;
	if (!f) { perror(path); return; }
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (i = 0; i < w * h; i++) {
		unsigned char rgb[3] = { (unsigned char)(p[i] >> 16), (unsigned char)(p[i] >> 8), (unsigned char)p[i] };
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
}

int main(int argc, char **argv)
{
	long frames, i, stall;
	FILE *wav = NULL;
	uint64_t samples = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <folder> <frames> [out.ppm] [mem.bin]\n", argv[0]);
		return 2;
	}
	if (chdir(argv[1]) != 0) { perror(argv[1]); return 2; }
	frames = strtol(argv[2], NULL, 0);
	stall = getenv("Q88_STALL_FRAME") ? strtol(getenv("Q88_STALL_FRAME"), NULL, 0) : -1;

	if (!press_load()) return 2;
	if (getenv("Q88_WAV") && !(wav = fopen(getenv("Q88_WAV"), "wb"))) { perror(getenv("Q88_WAV")); return 2; }
	/* 44320 Hz (0xad20), stereo, 16 bits: 177280 (0x2b480) bytes a second */
	if (wav) fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\x20\xad\0\0\x80\xb4\x02\0\x04\0\x10\0data\0\0\0\0", 1, 44, wav);
	if (!watch_load()) return 2;
#if defined(CHIMERA_MEMWATCH)
	if (g_watchCount) SetMemoryCallback((uint64_t)(uintptr_t)watch_called);	/* before Init, as the engine does */
#else
	if (g_watchCount) { fprintf(stderr, "Q88_WATCH: this build has no calls at memory (build-native.sh w)\n"); return 2; }
#endif
	if (!Init()) {
		fprintf(stderr, "Init failed: %s\n", GetLoadError());
		return 1;
	}
	fprintf(stderr, "vsync %d/%d\n", (int)GetVsyncNumerator(), (int)GetVsyncDenominator());
	for (i = 0; i < frames; i++) {
		int32_t d, lights = 0;
#if defined(CHIMERA_MEMWATCH)
		WATCH_APPLY(i, AddMemoryWatch, RemoveMemoryWatch);
#endif
		if (i == stall) usleep(300000);
		PRESS_APPLY(i, SetButton, SetAxis);
		FrameAdvance(0);
		samples += (uint64_t)GetAudioSampleCount();
		if (wav) fwrite(GetAudio(), 4, (size_t)GetAudioSampleCount(), wav);
		for (d = 0; d < GetDriveCount(); d++) lights |= GetDriveLight(d) << d;
		printf("%ld %016llx %dx%d %d %016llx %llu %d %x\n", i, (unsigned long long)GetFrameDigest(),
		       GetVideoWidth(), GetVideoHeight(), GetAudioSampleCount(),
		       (unsigned long long)GetAudioDigest(),
		       (unsigned long long)GetEmulatedClocks(), InputWasRead(), lights);
	}
	if (wav) {	/* the two sizes the header left open */
		uint32_t n = (uint32_t)(samples * 4), r = n + 36;
		fseek(wav, 4, SEEK_SET); fwrite(&r, 4, 1, wav);
		fseek(wav, 40, SEEK_SET); fwrite(&n, 4, 1, wav);
		fclose(wav);
	}
#if defined(CHIMERA_MEMWATCH)
	WATCH_REPORT(GetMemoryCallbackScopes(), GetMemoryCallbackReplaceable());
#endif
	if (getenv("Q88_REGS")) {
		int32_t r;
		printf("REGS");
		for (r = 0; r < GetRegisterCount(); r++)
			printf(" %s=%llx", GetRegisterName(r), (unsigned long long)GetRegisterValue(r));
		printf("\n");
	}
	fprintf(stderr, "%ld frames, %llu sound pairs, %llu clocks\n", frames,
	        (unsigned long long)samples, (unsigned long long)GetEmulatedClocks());
	if (argc > 3) write_ppm(argv[3]);
	if (getenv("Q88_SAVEDATA")) {
		int32_t k, nf = GetSaveDataFileCount();
		for (k = 0; k < nf; k++) {
			char path[1024];
			int64_t sz = GetSaveDataFileSize(k), done = 0, got;
			FILE *o;
			snprintf(path, sizeof path, "%s/%s", getenv("Q88_SAVEDATA"), GetSaveDataFileName(k));
			o = fopen(path, "wb");
			if (!o) { perror(path); return 4; }
			while (done < sz && (got = ReadSaveDataFile(k, done, sz - done)) > 0) {
				fwrite(GetSaveDataScratch(), 1, (size_t)got, o);
				done += got;
			}
			fclose(o);
			fprintf(stderr, "savedata %s %lld bytes\n", path, (long long)done);
		}
	}
	if (argc > 4) {
		FILE *f = fopen(argv[4], "wb");
		if (f) { fwrite(GetMemoryDomainPtr(0), 1, (size_t)GetMemoryDomainSize(0), f); fclose(f); }
	}
	return 0;
}
