/* memwatch.h - the calls at memory (Chimera issues 496 and 534): what the
 * driver's exports call in the watch build. See memwatch.c, and
 * docs/memory-events.md for what is and is not covered. */
#ifndef CHIMERA_MEMWATCH_H
#define CHIMERA_MEMWATCH_H

#include <stdint.h>

const char *chimera_watch_scopes(void);
const uint64_t *chimera_watch_pc(void);
void chimera_watch_set_callback(uint64_t fn);
int32_t chimera_watch_add(int32_t kind, int32_t scope, uint64_t address);
void chimera_watch_remove(int32_t kind, int32_t scope, uint64_t address);

#endif
