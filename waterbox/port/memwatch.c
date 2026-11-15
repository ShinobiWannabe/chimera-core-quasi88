/* memwatch.c - the calls at memory (Chimera issues 496 and 534), for the
 * watch build of the core (CHIMERA_MEMWATCH; the setting Lua Memory Events
 * picks it, q88-entry.c).
 *
 * A Lua script's event.on_bus_exec, on_bus_read and on_bus_write stop the
 * machine part way through a frame at an address one of the two Z80s uses.
 * Chimera hands the core a function (SetMemoryCallback, callback slot 4) and
 * names the places to watch (AddMemoryWatch, RemoveMemoryWatch); the core
 * calls the function as fn(kind, scope, address, value, size, replacement)
 * at each watched access: kind 0 an instruction about to run, 1 a read once
 * its value is fetched, 2 a write before memory changes. replacement points
 * at the value, which the script may change, for reads and writes.
 *
 * Two scopes: 0 "Main CPU", 1 "Sub CPU" (the disk unit's Z80). An address is
 * the 16-bit address the program used, as the CPU puts it on its bus,
 * whatever the banking maps there.
 *
 * The calls come from the hooks patches/0003 adds to QUASI88's Z80
 * (src/pc88/z80.c): M_RDMEM, M_WRMEM and the start of each instruction.
 *
 * Everything here is ECL_INVISIBLE: the function is the same slot address
 * in every session, and what is watched belongs to the scripts, not to the
 * machine, so no savestate carries it and a load leaves it as it is. No
 * emulated clock is spent, so a run with a script watching is the same run,
 * frame for frame, as one without. */
#if defined(CHIMERA_MEMWATCH)
#include <stdint.h>
#include <string.h>

#include <emulibc.h>

#include "quasi88.h"
#include "z80.h"
#include "pc88cpu.h"
#include "memwatch.h"

#define WATCH_MAX 1024
#define SCOPES 2

ECL_INVISIBLE int chimera_watch_read_on;
ECL_INVISIBLE int chimera_watch_write_on;
ECL_INVISIBLE int chimera_watch_exec_on;
/* above zero while inside a call to the engine: an access made then is not
 * reported again */
ECL_INVISIBLE static int g_depth;

ECL_INVISIBLE static uint64_t g_fn;
/* per scope and kind (0 exec, 1 read, 2 write): every address, or a map of
 * the 64 K addresses with a count of how many are marked */
ECL_INVISIBLE static int g_all[SCOPES][3];
ECL_INVISIBLE static int g_count[SCOPES][3];
ECL_INVISIBLE static uint8_t g_map[SCOPES][3][0x10000 / 8];
/* the address of the instruction each CPU is running, for
 * GetMemoryCallbackPc: Chimera reads it during a call */
ECL_INVISIBLE static uint16_t g_insn[SCOPES];
ECL_INVISIBLE static uint64_t g_pc;

static int scope_of(const z80arch *z80) { return z80 == &z80sub_cpu; }

static int marked(int scope, int kind, word addr)
{
	return g_all[scope][kind] || ((g_map[scope][kind][addr >> 3] >> (addr & 7)) & 1);
}

static void refresh(void)
{
	int s, any[3] = { 0, 0, 0 }, k;
	for (s = 0; s < SCOPES; s++)
		for (k = 0; k < 3; k++)
			any[k] |= g_all[s][k] || g_count[s][k] > 0;
	/* the instruction hook also runs while only reads or writes are
	 * watched: it notes each instruction's address for GetMemoryCallbackPc */
	chimera_watch_exec_on = g_fn != 0 && (any[0] || any[1] || any[2]);
	chimera_watch_read_on = g_fn != 0 && any[1];
	chimera_watch_write_on = g_fn != 0 && any[2];
}

typedef uintptr_t (*memory_fn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

static byte call(int kind, int scope, word addr, byte value)
{
	uint64_t now = value;
	g_pc = g_insn[scope];
	g_depth++;
	((memory_fn)(uintptr_t)g_fn)((uintptr_t)kind, (uintptr_t)scope, addr, value, kind == 0 ? 0 : 1,
		kind == 0 ? 0 : (uintptr_t)&now);
	g_depth--;
	return (byte)now;
}

byte chimera_watch_read(z80arch *z80, word addr, byte data)
{
	int s = scope_of(z80);
	if (g_depth || !marked(s, 1, addr)) return data;
	return call(1, s, addr, data);
}

byte chimera_watch_write(z80arch *z80, word addr, byte data)
{
	int s = scope_of(z80);
	if (g_depth || !marked(s, 2, addr)) return data;
	return call(2, s, addr, data);
}

void chimera_watch_exec(z80arch *z80)
{
	int s = scope_of(z80);
	g_insn[s] = z80->PC.W;
	if (g_depth || !marked(s, 0, z80->PC.W)) return;
	call(0, s, z80->PC.W, 0);
}

/* ------------------------------------------------------------ exports */

const char *chimera_watch_scopes(void) { return "Main CPU\nSub CPU\n"; }

const uint64_t *chimera_watch_pc(void) { return &g_pc; }

void chimera_watch_set_callback(uint64_t fn)
{
	g_fn = fn;
	refresh();
}

int32_t chimera_watch_add(int32_t kind, int32_t scope, uint64_t address)
{
	if (kind < 0 || kind > 2 || scope < 0 || scope >= SCOPES) return 0;
	if (address == UINT64_MAX)
		g_all[scope][kind] = 1;
	else if (address > 0xffff)
		return 0;
	else {
		uint8_t *b = &g_map[scope][kind][address >> 3], bit = (uint8_t)(1 << (address & 7));
		if (!(*b & bit)) {
			if (g_count[scope][kind] >= WATCH_MAX) return 0;
			*b |= bit;
			g_count[scope][kind]++;
		}
	}
	refresh();
	return 1;
}

void chimera_watch_remove(int32_t kind, int32_t scope, uint64_t address)
{
	if (kind < 0 || kind > 2 || scope < 0 || scope >= SCOPES) return;
	if (address == UINT64_MAX)
		g_all[scope][kind] = 0;
	else if (address <= 0xffff) {
		uint8_t *b = &g_map[scope][kind][address >> 3], bit = (uint8_t)(1 << (address & 7));
		if (*b & bit) {
			*b &= (uint8_t)~bit;
			g_count[scope][kind]--;
		}
	}
	refresh();
}
#endif /* CHIMERA_MEMWATCH */
