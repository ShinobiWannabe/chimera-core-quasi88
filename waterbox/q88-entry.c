/* q88-entry.c - the package's exports, over its two halves.
 *
 * core.wbx holds QUASI88 twice: as it is, with its driver (the half q88_),
 * and with the calls at memory (the half q88w_; Chimera issues 496 and 534,
 * port/memwatch.c), like the NP2kai core's two builds. Each half has
 * everything but its driver's entry points made local, so the two copies of
 * QUASI88 cannot meet. Init reads the "luaMemoryEvents" setting once and
 * every later call goes to that half; the other is never run, and its
 * memory never changes after the seal.
 *
 * The plain half is QUASI88's Z80 as it is: a Lua script's
 * event.on_bus_exec and the rest are refused there (Chimera asks for the
 * memory scopes before Init, and is told there are none). The watch half
 * tests a flag at every instruction and memory access, and runs the same
 * machine frame for frame. */
#include <stdint.h>

#include <emulibc.h>
#include <waterbox_settings.h>

#include "q88-exports.h"

#define DECL(h, ret, name, params) ret h##_##name params;
#define X(ret, name, params, args) DECL(q88, ret, name, params) DECL(q88w, ret, name, params)
Q88_EXPORTS(X)
#undef X
#define X(name, params, args) DECL(q88, void, name, params) DECL(q88w, void, name, params)
Q88_EXPORTS_BOTH(X)
#undef X
#define X(ret, name, params, args) DECL(q88w, ret, name, params)
Q88_EXPORTS_WATCH(X)
#undef X
int q88_Init(void);
int q88w_Init(void);

/* 0 the plain half, 1 the watch half. Chosen before the seal and never
 * changed, so states carry it. */
static int g_half;

static int memory_events(void) { return wbx_setting_bool("luaMemoryEvents", 0); }

ECL_EXPORT int Init(void)
{
	g_half = memory_events() ? 1 : 0;
	return g_half ? q88w_Init() : q88_Init();
}

#define X(ret, name, params, args) \
	ECL_EXPORT ret name params { return g_half ? q88w_##name args : q88_##name args; }
Q88_EXPORTS(X)
#undef X

/* What Chimera hands the core before Init, when the half is not chosen yet
 * (its engine sets the input-poll callback first, session.cpp): both halves
 * take it, so the half Init picks has it. */
#define X(name, params, args) \
	ECL_EXPORT void name params { q88_##name args; q88w_##name args; }
Q88_EXPORTS_BOTH(X)
#undef X

/* ---- the calls at memory. Chimera asks for the scopes and hands over its
 * function before Init (session.cpp), so these read the setting
 * themselves; watches come after Init, to the half it chose. With the
 * setting off there are no scopes, and Chimera refuses the events with its
 * own words ("This core cannot call a Lua function at ..."). */
ECL_EXPORT const char *GetMemoryCallbackScopes(void)
{
	return memory_events() ? q88w_GetMemoryCallbackScopes() : "";
}

ECL_EXPORT int32_t GetMemoryCallbackReplaceable(void)
{
	return memory_events() ? q88w_GetMemoryCallbackReplaceable() : 0;
}

ECL_EXPORT const uint64_t *GetMemoryCallbackPc(void)
{
	return memory_events() ? q88w_GetMemoryCallbackPc() : 0;
}

ECL_EXPORT void SetMemoryCallback(uint64_t fn) { q88w_SetMemoryCallback(fn); }

ECL_EXPORT int32_t AddMemoryWatch(int32_t kind, int32_t scope, uint64_t address)
{
	return g_half ? q88w_AddMemoryWatch(kind, scope, address) : 0;
}

ECL_EXPORT void RemoveMemoryWatch(int32_t kind, int32_t scope, uint64_t address)
{
	if (g_half) q88w_RemoveMemoryWatch(kind, scope, address);
}
