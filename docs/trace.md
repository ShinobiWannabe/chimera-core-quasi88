# The trace logger

Chimera's Trace Logger (Emulator > Trace Logger), and the MCP server's
`trace_cpu`, show a line for each instruction the machine runs. This core
gives a line for every instruction of both Z80s: the main CPU and the disk
unit's CPU. It needs no setting: both copies of QUASI88 in the package (with
Lua Memory Events off and on, docs/memory-events.md) have it.

## A line

    Main B8B0: 3E0B      LD    A,0BH	AF:FF84 BC:02FD DE:D21A HL:0700 IX:98BD IY:4792 SP:89F7
    Sub 07A7: CB57      BIT   2,A	AF:0411 BC:0600 DE:007E HL:5906 IX:FFFF IY:FFFF SP:7FFA

- the CPU: `Main`, or `Sub` for the disk unit's Z80;
- the address of the instruction, as the CPU sees it (whatever the banking
  maps there);
- the instruction's bytes, and the instruction as QUASI88's own debugging
  monitor writes it (its disassembler, `src/pc88/z80-debug.c`);
- after a tab, the registers as the instruction starts. The program counter
  is the address at the start of the line.

Lines come in the order the instructions ran. QUASI88 runs the two CPUs in
turns, so a run of `Main` lines is followed by a run of `Sub` lines.

## The machine is the same

Writing a line costs the machine nothing: the run is the same frame for
frame with the trace on or off, so a movie plays back the same either way.
To get there the instruction's bytes are not read the way the CPU reads
them. On the main CPU a real read may add memory wait time, and a read of
graphics memory through the ALU changes the ALU's latch; the trace reads
the bytes with a read that does neither (`chimera_main_peek`). The disk
unit's read changes nothing, so it is read as it is.

With the trace off, each instruction costs one test of a flag; no
difference in speed could be measured (Xevious in the sandbox: 1781 frames
a second against 1768 for the build before the trace).

## How it is built

- `patches/0004-trace-hooks.patch`, under `CHIMERA_CORE` (both builds):
  - `src/pc88/z80.c`: before each instruction, while the trace is on, a call
    to `chimera_trace` (after the memory event hook, so a script's
    instruction event comes first).
  - `src/pc88/z80-debug.c`: the disassembler compiled without the rest of
    the monitor (which reads the host's terminal), reading bytes with
    `chimera_trace_peek`; its scratch buffer is kept out of savestates.
  - `src/pc88/pc88main.c`: `chimera_main_peek`, `main_mem_read` without the
    ALU's latch.
- `waterbox/port/trace.c`: the line buffer and Chimera's trace exports
  (`TraceSetEnabled`, `TraceGetLineCount`, `TraceGetBuffer`,
  `TraceGetUsedBytes`, `TraceGetOverflow`, `TraceClear`, `TraceGetHeader`).
  The buffer is 8 MiB, allocated with `alloc_invisible`, and the flag and
  counts are `ECL_INVISIBLE`: no savestate holds the trace, and Chimera says
  again after a load whether to trace. Chimera empties the buffer after
  every frame; lines that do not fit in one frame are dropped, and
  `TraceGetOverflow` says so. The most measured was about 18,000 lines a
  frame (CPU timing 2), about 1.7 MB.
- `waterbox/q88-exports.h`: `TraceSetEnabled` goes to both halves, since
  Chimera turns the trace off before Init; the rest go to the half Init
  chose.

## Tested

The gate traces 50 frames of Xevious with memory waits on: the traced run
equals the untraced run (picture, sound, clock and main RAM), native and
sandbox give the same lines byte for byte, both CPUs have lines, none was
dropped and every line has the form above, and the watch half traces the
same lines. Broken on purpose (a line costing the machine one clock), the
first of these fails. The harnesses take `Q88_TRACE=file[,from]`
(`waterbox/trace-script.h`).
