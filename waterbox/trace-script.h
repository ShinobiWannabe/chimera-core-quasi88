/* trace-script.h - Q88_TRACE for the two harnesses (run-native.c and
 * run-wbx.c): the engine's side of the trace logger (port/trace.c).
 *
 *   Q88_TRACE=file[,from]   trace from frame <from> (0 when left out) to the
 *                           end, and write the lines to <file>, one a line
 *
 * The harness turns the trace off before Init, as Chimera's engine does
 * (session.cpp), turns it on before frame <from>, and after each frame
 * copies the lines out and empties the buffer. At the end it prints
 * "TRACE <lines> lines, <frames with lines dropped> overflowed" on stderr,
 * so the frame lines on stdout stay what an untraced run prints. */
#ifndef Q88_TRACE_SCRIPT_H
#define Q88_TRACE_SCRIPT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *g_traceFile;
static long g_traceFrom, g_traceLines, g_traceOverflows;

static int trace_load(void)
{
	char path[1024];
	const char *v = getenv("Q88_TRACE"), *comma;
	size_t n;
	if (!v || !*v) return 1;
	comma = strchr(v, ',');
	n = comma ? (size_t)(comma - v) : strlen(v);
	if (n >= sizeof path) n = sizeof path - 1;
	memcpy(path, v, n);
	path[n] = 0;
	g_traceFrom = comma ? strtol(comma + 1, NULL, 0) : 0;
	if (!(g_traceFile = fopen(path, "wb"))) { perror(path); return 0; }
	return 1;
}

/* after a frame: the lines (each ended by a NUL in the core's buffer) out */
static void trace_take(const char *buf, int32_t used, int32_t lines, int32_t overflow)
{
	int32_t i;
	if (!g_traceFile) return;
	for (i = 0; buf && i < used; i++) fputc(buf[i] ? buf[i] : '\n', g_traceFile);
	g_traceLines += lines;
	if (overflow) g_traceOverflows++;
}

static void trace_report(void)
{
	if (!g_traceFile) return;
	fclose(g_traceFile);
	fprintf(stderr, "TRACE %ld lines, %ld overflowed\n", g_traceLines, g_traceOverflows);
}

#define TRACE_BEFORE_INIT(SetEnabled) do { if (g_traceFile) SetEnabled(0); } while (0)
#define TRACE_BEFORE_FRAME(n, SetEnabled) do { if (g_traceFile && (n) == g_traceFrom) SetEnabled(1); } while (0)
#define TRACE_AFTER_FRAME(Buffer, Used, Lines, Overflow, Clear) do { \
	if (g_traceFile) { \
		trace_take((const char *)(Buffer()), Used(), Lines(), Overflow()); \
		Clear(); \
	} \
} while (0)

#endif
