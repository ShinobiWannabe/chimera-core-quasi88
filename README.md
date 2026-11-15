# chimera-core-quasi88

[QUASI88](https://github.com/bubio/QUASI88), the NEC PC-8801 emulator, as a
core for [Chimera](https://github.com/ToolAssisted-run/chimera), the frontend
for tool-assisted speedruns. It emulates a PC-8801mkIISR and later models:
both Z80s (the main CPU and the disk unit's), the text and graphics screens,
the YM2203 (sound board I) or YM2608 (sound board II), the beeper, the
keyboard, a joystick or a mouse, and two floppy drives with D88 disks.

The core runs on one clock inside Chimera's sandbox, so a movie plays back
the same every time: the sandboxed build and the native reference build agree
frame for frame on picture, sound and memory (docs/PLAN.md says what was
measured).

## Using it

Put `quasi88-<id>.chimeraCore` in Chimera's `Cores` folder, then make a new
project with your floppy disk images (`.d88`) and cassette tapes (`.t88`,
`.cmt`). You also need the firmware of a PC-8801 you own:

- `PC88.ROM`, the combined BASIC and disk unit ROMs that M88 and QUASI88 read
  (the default), or QUASI88's separate files (`N88.ROM`, `N88_0.ROM` to
  `N88_3.ROM`, `N88N.ROM`, `DISK.ROM`) with the ROM Files setting changed.
  The files of a set must all come from one machine.
- `KANJI1.ROM`, and `KANJI2.ROM` unless the Level 2 Kanji ROM setting is off.
- For the drums of Sound Board II, only with the Rhythm Samples setting on:
  `ym2608_adpcm_rom.bin` (MAME's generator) or the six `2608_*.WAV` (fmgen).

Some games need another BASIC mode (Xevious asks for V1) or CPU clock; those
are settings of the project.

For a TAS, the core has Chimera's trace logger for both Z80s
([docs/trace.md](docs/trace.md)) and, with the Lua Memory Events setting,
Lua events at memory ([docs/memory-events.md](docs/memory-events.md)).

## Building

See [docs/BUILDING.md](docs/BUILDING.md). In short, on Linux with a Chimera
checkout beside this one:

    git submodule update --init
    waterbox/build-package.sh -r ../chimera-main

## Licence

This repository's own files are MIT. The package also holds QUASI88
(BSD-3-Clause), the MAME sound drivers QUASI88 carries (the old MAME licence,
which forbids commercial use) and fmgen (its own terms); see
[LICENSE](LICENSE) and `waterbox/package-licenses.json`.
