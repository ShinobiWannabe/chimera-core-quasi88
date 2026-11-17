/* chimera-port.h - the calls between the port layer (waterbox/port), the
 * driver (waterbox/q88-driver.c) and the hooks upstream's patches add. */
#ifndef CHIMERA_PORT_H
#define CHIMERA_PORT_H

#include <stddef.h>
#include <stdint.h>

/* ---- the picture (port/graph.c) ---- */
/* The screen QUASI88 draws, B, G, R, A in memory, and its size. */
const uint32_t *chimera_screen(int *width, int *height);

/* ---- the sound (port/audio.c) ---- */
/* The rate the core mixes at, and the samples one frame makes: 44320 Hz is
 * 800 samples a frame at QUASI88's 55.4 frames a second. */
#define CHIMERA_SOUND_RATE 44320
#define CHIMERA_SOUND_FRAME 800
/* The frame's sound, as interleaved stereo pairs; the count. */
int chimera_sound_take(int16_t *out, int maxPairs);

/* ---- input (q88-driver.c, called from the patched upstream) ---- */
/* The machine read the keyboard or the controller port. */
void chimera_input_read(void);

/* ---- the clock (q88-driver.c) ---- */
/* Seconds of emulated time since power-on, added to the Start Time setting:
 * pc88main.c's calendar asks for the time through these (q88-flags.sh). */
long long chimera_time_now(void);

/* ---- the files (port/file-op.c) ---- */
/* Files the machine opened for writing, which leave as save data. */
int chimera_file_count(void);
const char *chimera_file_name(int i);
int chimera_file_written(int i);
int64_t chimera_file_size(int i);
const uint8_t *chimera_file_data(int i);
/* Reads the whole of a mounted file into the file layer, so a disk the
 * machine writes is written in guest memory; 0 when it is not mounted. */
int chimera_file_exists(const char *name);
/* The size of a mounted file, read in as chimera_file_exists does; -1 when
 * it is not mounted. */
int64_t chimera_file_length(const char *name);
/* a mounted file's bytes, read in on first use (NULL when not mounted) */
const uint8_t *chimera_file_bytes(const char *name, int64_t *len);
/* A file that is not opened even if it is mounted: a ROM the settings
 * leave out of the machine. */
void chimera_file_hide(const char *name);

#endif
