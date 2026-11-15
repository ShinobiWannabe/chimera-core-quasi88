/* trace.c - the trace logger: a line for each instruction either Z80 runs
 * while Chimera's Trace Logger (or another reader of the trace, such as the
 * MCP server's trace_cpu) is on.
 *
 * The hook is in QUASI88's Z80 loop (patches/0004, src/pc88/z80.c): before
 * each instruction, while chimera_trace_on is set, it calls chimera_trace.
 * Both builds of the core have it (CHIMERA_CORE), so the trace does not
 * need the Lua Memory Events setting. A line is
 *
 *   Main 0869: C3B303   JP    03B3H<tab>AF:0044 BC:0000 DE:... SP:F000
 *
 * the CPU ("Main" or "Sub", the disk unit's Z80), the address, the
 * instruction's bytes and QUASI88's own disassembly of it (its monitor's,
 * src/pc88/z80-debug.c), then after a tab the registers as the instruction
 * starts. The bytes are read with chimera_trace_peek, which has none of a
 * real read's side effects: the main CPU's read adds memory waits and,
 * through the ALU, latches the VRAM planes, and the trace must leave the
 * machine as it was.
 *
 * The lines go into one buffer, each ended by a NUL, which Chimera reads
 * after every frame and empties (TraceClear). It is allocated with
 * alloc_invisible, and the flag and counts are ECL_INVISIBLE: no savestate
 * carries the trace, and Chimera says again after each load whether to
 * trace. When a frame's lines do not fit, the rest are dropped and
 * TraceGetOverflow says so. Nothing here costs emulated time: a run traced
 * is the same run, frame for frame, as one that is not. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <emulibc.h>

#include "quasi88.h"
#include "z80.h"
#include "pc88cpu.h"
#include "memory.h"
#include "trace.h"

/* At 8 MHz, the two Z80s run about 50,000 instructions a frame between
 * them, about 90 bytes a line. */
#define TRACE_BYTES (8 << 20)

ECL_INVISIBLE int chimera_trace_on;
ECL_INVISIBLE static char *g_trace;
ECL_INVISIBLE static int32_t g_used, g_lines, g_overflow;

byte chimera_main_peek(word addr);	/* patches/0004, src/pc88/pc88main.c */
int z80_line_disasm(z80arch *z80, word pc, char output[36]);	/* src/pc88/z80-debug.c */

void chimera_trace_init(void)
{
	if (!g_trace) g_trace = (char *)alloc_invisible(TRACE_BYTES);
}

byte chimera_trace_peek(z80arch *z80, word addr)
{
	if (z80 == &z80main_cpu) return chimera_main_peek(addr);
	return sub_romram[addr & 0x7fff];	/* the sub CPU's read changes nothing */
}

void chimera_trace(z80arch *z80)
{
	char dis[64], line[160];
	int n;
	if (!g_trace) return;
	memset(dis, 0, sizeof dis);
	z80_line_disasm(z80, z80->PC.W, dis);
	/* "AAAA CCCCCCCC NNNN...": the address, the bytes, the instruction,
	 * padded with spaces at the end */
	n = (int)strlen(dis);
	while (n > 0 && dis[n - 1] == ' ') dis[--n] = 0;
	n = snprintf(line, sizeof line,
		"%s %.4s:%s\tAF:%04X BC:%04X DE:%04X HL:%04X IX:%04X IY:%04X SP:%04X",
		z80 == &z80main_cpu ? "Main" : "Sub", dis, dis + 4,
		z80->AF.W, z80->BC.W, z80->DE.W, z80->HL.W, z80->IX.W, z80->IY.W, z80->SP.W);
	if (n < 0 || n >= (int)sizeof line) return;
	if (g_used + n + 1 > TRACE_BYTES) {
		g_overflow = 1;
		return;
	}
	memcpy(g_trace + g_used, line, (size_t)n + 1);
	g_used += n + 1;
	g_lines++;
}

/* ---- Chimera's trace exports (its session.cpp; the synthetic core's
 * synth_wbx.c is the example) */
ECL_EXPORT void TraceSetEnabled(int32_t on) { chimera_trace_on = on != 0; }
ECL_EXPORT int32_t TraceGetLineCount(void) { return g_lines; }
ECL_EXPORT const char *TraceGetBuffer(void) { return g_trace; }
ECL_EXPORT int32_t TraceGetOverflow(void) { return g_overflow; }
ECL_EXPORT int32_t TraceGetUsedBytes(void) { return g_used; }
ECL_EXPORT void TraceClear(void) { g_used = g_lines = g_overflow = 0; }
ECL_EXPORT const char *TraceGetHeader(void)
{
	return "CPU address: bytes instruction\tregisters as it starts";
}
