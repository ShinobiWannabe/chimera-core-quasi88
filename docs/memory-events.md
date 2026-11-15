# Lua memory events

Chimera's Lua scripts can stop part way through a frame at memory the
machine runs, reads or writes: `event.on_bus_exec`, `on_bus_exec_any`,
`on_bus_read` and `on_bus_write` (Chimera issues 496, 534 and 571). This
core offers them when the setting **Lua Memory Events** is on. It is off by
default, as in the NP2kai core.

## The setting

QUASI88 is in the package twice: as it is, and a copy that checks every
instruction and every memory access of both Z80s (q88-entry.c picks the
copy when the machine starts). With the setting off the core runs at full
speed, and Chimera refuses the events with its own message ("This core
cannot call a Lua function at ..."). With it on, the core is about 3%
slower (Xevious in the sandbox: about 1765 frames a second against 1813).

The machine is the same either way. Every frame, the sound, the clock and
the memory are the same, also with every address of both CPUs watched, so a
movie plays back the same with the setting on or off. The gate checks this.

## What a script sees

- **Two scopes**: "Main CPU" (scope 0) and "Sub CPU" (scope 1, the disk
  unit's Z80). An address is the 16-bit address the program used, as the
  CPU puts it on its bus: what the PC-8801's banking maps there (RAM, ROM,
  graphics VRAM) is not part of it. The core's memory domains are the
  memories themselves, so an address found with RAM Search in "Main RAM" is
  the CPU address only where that RAM is mapped (it always is at 8000h and
  above, and below 8000h when the program has switched the ROM out).
- **Instructions** (`on_bus_exec`): called before the instruction runs, at
  the address of its first byte, with the registers as they are then.
- **Reads and writes** (`on_bus_read`, `on_bus_write`): one call per byte,
  size 1, with its value. A read is reported once the value is fetched, and
  a write before memory changes. The script may give back another value, and
  the CPU reads or writes that instead.
- **The instruction that made an access** (`GetMemoryCallbackPc`): the
  address of the instruction each CPU is running, so Chimera's MCP server
  can say which instruction wrote a value.
- Watches stay through state loads. No savestate carries them.

## Not covered

- The opcode bytes of an instruction are not reported as reads (QUASI88
  fetches them through its own fetch function). The bytes that follow the
  opcode, an immediate value or an address, are reported as reads.
- Input and output ports (IN and OUT): they are not memory.
- Memory the CPUs do not touch themselves: the text screen's DMA, the
  disk controller's transfers, and the memory tools' own pokes.

## How it is built

- `patches/0003-memory-watch-hooks.patch`: under `CHIMERA_MEMWATCH`,
  QUASI88's Z80 (`src/pc88/z80.c`) checks a flag at each memory read and
  write and at the start of each instruction. The plain build has none of
  it.
- `waterbox/port/memwatch.c`: what is watched (up to 1024 addresses per
  scope and kind, or every address), and the call into Chimera. It is all
  `ECL_INVISIBLE`.
- `waterbox/build-guest.sh` builds QUASI88 twice, links each into one
  relocatable half with its exports renamed (q88_ and q88w_) and the rest
  made local, and `waterbox/q88-entry.c` passes each call to the half the
  setting chose (`waterbox/q88-exports.h` lists the exports).
- Test harnesses: `waterbox/build-native.sh w` builds the native watch
  build; `Q88_WATCH` in `run-native` and `run-wbx`
  (`waterbox/watch-script.h`).
