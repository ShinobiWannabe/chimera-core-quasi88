# PLAN - the QUASI88 core

The design log of this core: what was decided, why, and what was measured.
Dated sections; the newest decisions are at the end of each section.

## 1. The request (2026-11-14)

A core for the NEC PC-8801, built like the NP2kai core
(chimera-core-np2kai), from QUASI88.

- **Upstream**: https://github.com/bubio/QUASI88, the SDL2 port of Showzoh
  Fukunaga's QUASI88, pinned at c5f959a ("Fix q8tk signal callback types").
  It is C, with fmgen in C++.
- **Licences**: QUASI88 itself is BSD-3-Clause. It compiles in the MAME/XMAME
  0.106 sound layer (src/snddrv/xmame), under the old MAME licence (no
  selling, no commercial product or activity, a modified build is
  distributed with its complete source), and fmgen, under its own terms
  (free software, origin named, changes stated, its text unchanged;
  commercial software needs the author's consent). The NP2kai core already
  ships fmgen under the same terms. The owner accepted the MAME licence's
  non-commercial terms for this core (user-decided, 2026-11-14); the
  package links the source of every part by commit.
- **One thread on one clock**: QUASI88 is single threaded. Its main loop
  (quasi88.c quasi88_loop) runs the two Z80s until the next VSYNC and returns;
  the only waits are in the system layer, which the core replaces.
- **The picture**: the machine's own framebuffer, drawn by QUASI88 into a
  buffer the core gives it (way 1 of porting-a-core.md). No GPU.
- **The user brings**: the BASIC and disk ROMs (PC88.ROM, or the separate
  files) and the kanji ROMs.

## 2. The shape (2026-11-14)

- `extern/QUASI88` is the submodule; `patches/` holds the changes. There are
  four (0003 and 0004 are sections 14 and 15). `0001-input-poll-hooks.patch` adds two calls to `chimera_input_read()`
  under `CHIMERA_CORE`, where the machine reads its keyboard ports
  (pc88main.c main_io_in, ports 00h-0Fh) and its controller port (soundbd.c
  sound_in_data, registers 0Eh and 0Fh). They mark the frame as one that read
  its controls (lag frames) and call the engine's input-poll callback.
  `0002-drive-access-hook.patch` marks a floppy drive as used where the disk
  controller reads or writes its disk, for the drive lights (section 9c).
- `waterbox/port` is a system layer of QUASI88's own kind, modelled on
  upstream's `src/sysdepend/mini` and `src/osdepend/dummy`: config.h,
  device.h, filename.h, graph.c (the screen), system.c (events, the wait,
  main.c's functions), audio.c (the MAME layer's sound output), file-op.c
  (files), clock.c (the calendar's time).
- `waterbox/q88-driver.c` is the Chimera ABI.

## 3. Starting the machine (2026-11-14)

The driver builds a QUASI88 command line from the settings and hands it to
upstream's own `config_init`, then calls `quasi88_start`, as mini/main.c
does. So a setting means exactly what the same option means to QUASI88
(getconf.c), and nothing of QUASI88's configuration is rewritten. Always on
the line: `-noconfig -nostatus -notool -full -frameskip 1 -noautoskip
-nowait -nofocus -sound -samplefreq 44320 -nosamples`.

The project's floppies arrive in the `floppy` slot. A D88 file can hold
several disks; the driver counts them from the headers (the size at 1Ch) and
each drive's media list is every disk of every file. At power-on drive 1
has the first disk and drive 2 the second file, or the first file's second
disk, as QUASI88's own start does (fname.c imagefile_all_open).

## 4. Time (2026-11-14)

One FrameAdvance is one `quasi88_loop`: one VSYNC at QUASI88's 55.4 Hz
(initval.h CONST_VSYNC_FREQ), so the frame rate is 277/5. QUASI88's own wait
(wait_vsync_update) returns at once and says "on time", so its automatic
frame skip never skips.

The only host clock QUASI88 reads is the calendar clock's (pc88main.c,
time/localtime/mktime). q88-flags.sh compiles that file with those names
changed to clock.c's, which give the Start Time setting plus the emulated
time (a frame is 5/277 s) and read it in UTC, so the clock shows the setting
as written whatever the host's time zone.

The alarms QUASI88 keeps for its interface count one frame a frame
(system.c wait_calc_elasped_time_ms).

Measured: a 300 ms stall of the host before frame 300 leaves the 900 frames
of Master of Monsters identical (the gate's time test). That test has not
been seen to fail: nothing in the build reads the host's clock to make it
fail with.

## 5. The picture (2026-11-14)

QUASI88 is asked for its full-size screen without toolbar or status bar:
640x400, the 200-line modes drawn line doubled. It draws only what changed
since the last frame, into the buffer graph.c gives it.

The pixel values carry no alpha: QUASI88 composes the text over the
graphics with bit operations and takes black to be 0, as SDL's pixel values
without alpha are. The driver copies the screen into the BGRA buffer Chimera
reads and sets the alpha; turbo (SetRenderingEnabled) skips only that copy.

## 6. Sound (2026-11-14)

The MAME layer mixes a frame at a time and hands it to
`osd_update_audio_stream` (audio.c). The rate is 44320 Hz, exactly 800
samples a frame at 55.4 Hz, so every frame has the same number of samples.
Both generators (MAME's and fmgen) and both sound boards are built in.

Measured: the native and sandboxed builds give the same sound, sample for
sample, with MAME and with fmgen (Master of Monsters' title music, 1800
frames, sound board I and II). Over those frames the generator and the board
changed no picture, clock or memory; the settings are still declared part of
the machine, as the chip's status reads go through the generator.

## 7. Input (2026-11-14)

Chimera sends levels. Each frame the driver writes QUASI88's keyboard ports
(`key_scan`, ports 00h-0Eh) whole from what is held, as the keyboard's
matrix would read: each key pulls its bits low (q88-input.h, written by
tools/gen-config.py from QUASI88's keyport tables). The keys of the later
keyboards that older software reads as another key pull that key's bit too,
and the ones that read as SHIFT plus a key pull SHIFT, as the real keyboard
does. KANA and CAPS are locking keys: a press turns the lock on or off. The
joystick (and the mouse buttons, which are the same bits) is port 0Fh of
`key_scan`, which QUASI88's own keyboard_update copies to the sound chip's
port. The mouse moves through keyboard_mouse_move.

This bypasses QUASI88's host key translation (quasi88_key), whose function
keys open its menus and whose romaji input would type for the player.

Measured: SPACE starts Xevious and the joystick moves the ship, native and
sandbox identical, and the run differs from one without the input.

## 8. Files and save data (2026-11-14)

A core sees only the files Chimera mounts, read-only. file-op.c reads each
file QUASI88 opens whole into guest memory the first time and serves it from
there, so a disk the machine writes is written in memory, is part of every
savestate, and leaves as save data (the whole image under its own name).
QUASI88's configuration, state, snapshot, tape, printer and serial files are
refused as if absent.

## 9. Firmware (2026-11-14)

QUASI88 reads the BASIC and disk ROMs either as separate files (N88.ROM,
N88_0.ROM to N88_3.ROM, N88N.ROM, DISK.ROM) or as one combined file in M88's
layout (its -compatrom option). The ROM Files setting chooses; PC88.ROM is
the default.

Measured with the owner's firmware folder: its separate files come from
different machines (n88.rom, n88_0.rom and n88_2.rom differ from the same
parts of its PC88.ROM, n88_1 and n88_3 match, and its N88EXT0-3.ROM are yet
another set). With them Xevious and Dig Dug start but show garbage text
attributes over the screen; with PC88.ROM both are right. A mixed set is the
likeliest firmware a user has, so the combined file is the default and the
setting's description warns about mixing.

The kanji ROMs are separate either way. The font comes from KANJI1.ROM, as
QUASI88 does it when no FONT.ROM is given, so no font file is declared.

## 9a. Every format QUASI88 reads (2026-11-14)

What QUASI88 opens, from every osd_fopen call in the sources it compiles:

| QUASI88 opens | In the core |
|---|---|
| Floppy disks, D88 (any name; .d88 and .88d by custom) | the `floppy` slot |
| Cassette tapes to load, T88 or raw CMT (pc88main.c sio_open_tapeload, which tells them apart by T88's header) | the `tape` slot (added 2026-11-14) |
| A cassette tape to save to (sio_open_tapesave, appended) | tape-save.cmt, made in memory and exported as save data (added 2026-11-14) |
| The BASIC, disk unit, kanji and dictionary ROMs, separate or combined (-compatrom) | firmware |
| The YM2608 drum ROM, ym2608_adpcm_rom.bin (MAME's generator), or the six 2608_*.WAV recordings (fmgen) | firmware, when the new Rhythm Samples setting is on (added 2026-11-14) |
| Its configuration and key binding files, state files (.sta), snapshots, wave recordings, key record and playback files, printer and serial port files | refused: the project, Chimera's savestates and movies stand for them, and nothing is written to the host |

Tapes: the first tape is in the data recorder at power-on; Tape Previous,
Next, Swap and Eject change it as the drive controls change disks, and Tape
Rewind winds it back (control.c quasi88_load_tape_rewind). The recorder is
listed as a drive, its light the cassette motor relay. The gate's tape test
needs only the firmware: it types a one-line program in ROM BASIC, saves it
with SAVE "CAS:A", gives the saved tape back as a project tape, loads it with
LOAD "CAS:A" and runs it, native against sandbox, with the same typing and
no tape as the control. Measured: a 37-byte tape, loaded and run, identical
in both builds.

Rhythm Samples is off by default, so no firmware is asked for until a
project wants the drums; with it off the drum files are not read even if
mounted. Not tried: no drum ROM or WAV files were at hand.

## 9b. Review fixes (2026-11-14)

- A disk that failed to go into a drive (Swap) was still shown as inserted,
  though QUASI88 had already taken the old disk out: the drive is now shown
  empty.
- A disk or tape QUASI88 could not open at power-on was shown as inserted:
  the media state is now read back from QUASI88 after it starts.
- The file layer held at most 64 files and the save data list 64 entries;
  both now hold 512, more than the slots file can name.
- A hidden ROM is hidden in either case, since QUASI88 asks for some names
  in capitals first.
- tools/gen-config.py refuses two columns of one player with the same
  header, as Chimera's contract tests do.

## 9c. The owner's answers (user-decided, 2026-11-14)

- **One disk, one drive.** Swap puts the selected disk in; when that disk
  is in the other drive, the two drives trade disks, as the NP2kai core
  does. An empty drive gives nothing back: the other drive is left empty.
  QUASI88 itself would have put one disk in both drives. Checked with
  run-wbx's Q88_MEDIA: two files, drive 2 swapped to drive 1's disk, the
  drives hold each other's.
- **Drive lights for any access.** `patches/0002-drive-access-hook.patch`
  calls `chimera_drive_access(fdc.us)` where the disk controller reads or
  writes a disk (fdc.c fdc_search_id, fdc_read_data, fdc_write_data,
  fdc_write_id); a drive's light is on for a frame in which that happened.
  The first try hooked fdc_ctrl whenever a command was in progress, and lit
  the drives on every frame: the disk unit's ROM keeps issuing status
  commands. Measured now: Choplifter's drive is lit on 112 of its first
  2400 frames.
- **The licence.** The MAME licence's non-commercial terms are acceptable
  for this core.
- A mistake the light measurement showed: the gate's lag-frame test read the
  lights' field instead of the controls' one. It now reads the right field
  and passes.

## 10. Results (2026-11-14)

Native against sandbox, every frame's picture, sound, clock and drive lights
and the main RAM at the end, identical (the gate):

| Game | Frames | Settings | Seen |
|---|---|---|---|
| Xevious | 1500 | BASIC V1S | title, then play with input |
| Master of Monsters | 1800 | default; sound board II with fmgen | title with music |
| Dig Dug (SR) | 1200 | default | title |
| Xanadu | 1500 | default | opening |
| Choplifter | 1200 | default | attract mode |
| Bomb Jack | 900 | default | title |

The sandboxed core runs at about 1500 frames a second on the owner's PC.
A savestate taken in play and loaded gives the straight run again (7.9 MB a
state); one extra press after the load shows (the control).

Through Chimera's engine (chimera-run built from Chimera a0b446f8 on Linux,
without the window): a project made with `--new-project` from Xevious and an
1800-frame movie (SPACE, then the joystick) plays to the same main RAM as the
native reference given the same input, and so does the same movie with
`--rerecord` (a state saved and loaded around every frame). The firmware
check and the load error ("The firmware file pc88.rom is missing.") come
through in the package's texts.

Not done yet: the frontend gate (`waterbox/tests/run-frontend.sh`, which
needs Chimera's window built with Qt) and Chimera's contract tests
(`test-packages`, also built with the window), a movie recorded in the
window and played on Windows, the CI workflow, and translations of the texts
(English only; Chimera shows English where a table is missing).

## 11. Disk Access Time (user-decided, 2026-11-14)

Disk Access Time (QUASI88's -fdc_wait) stays on by default, for timing
closer to a real PC-8801; QUASI88's own default is off. The setting's
description says that turning it off is not as accurate to real hardware.
Measured, the frame of the last disk read in the first 2400, on against
off: Choplifter 477 against 373, Xevious 306 against 291, Master of
Monsters 313 against 294, Xanadu 1387 against 1359, Bomb Jack 2392 against
2368, Dig Dug 1257 against 1238. Neither has been checked against a real
machine.

## 12. Controls as in the NP2kai core (user-decided, 2026-11-14)

The owner asked for the bindings and defaults to be as close to the NP2kai
core's as possible. Every control the two cores share now has the NP2kai
core's name, letter, header and default binding, the other controls are in
its order (the mouse buttons, the joystick, the drives, Reset), and TAStudio
shows the same columns at first. In particular: STOP is Shift+Pause alone,
RETURN takes Enter and the keypad's Enter, CTRL leaves Right Ctrl free, the
underscore and keypad comma ship unbound, letters, digits and signs carry no
header of their own. Checked by script:
no shared control differs in name, letter, header or binding, and no PC key
is bound twice.

What the PC-8801 has and the PC-98 has not keeps its own name: HENKAN takes
the PC-98's XFER binding (Alt+Period), KETTEI its NFER one (Alt+Comma); INS
DEL (the older keyboards' one key), PC, ZENKAKU and the keypad's RETURN ship
unbound; the tape controls are lettered and headed as the NP2kai core's CD
drive controls. The keys stay in the PC-8801 keyboard's order.

## 13. The mouse's range (user-decided, 2026-11-15)

The mouse axes take -127 to 127 a frame, not the NP2kai core's -180 to 180.
The PC-8801's mouse hands over its movement as one signed byte an axis at
each read, and QUASI88 clips each read to that (pc88/keyboard.c
keyboard_jop1_strobe); games read it about once a frame. A larger movement
was not lost but carried to the next read, a frame late.

## 14. Lua memory events, two builds (user-decided, 2026-11-15)

The owner asked for a Lua Memory Events setting built as the NP2kai core
builds it: the package holds QUASI88 twice, once as it is and once with
hooks for the bus events, and the setting picks one when the machine
starts. docs/memory-events.md says what a script sees and how it is built.
`patches/0003-memory-watch-hooks.patch` adds the hooks to QUASI88's Z80.

Measured (the gate): the watch build with nothing watched runs the plain
build's machine, native and sandbox; with every address of both CPUs
watched for instructions, reads and writes the machine is unchanged, and
native and sandbox make the same 12905929, 4355855 and 632695 calls over
900 frames of Xevious; replacing every write with 0 changes the machine
(the control); with the setting off the core refuses watches. The watch
half is about 3% slower (1765 against 1813 frames a second).

## 15. The trace logger (2026-11-15)

The owner asked for trace logging, as the NP2kai core has just added. That
core's change was not published where this one could read it, so this
follows Chimera's own trace protocol (its session.cpp, and the synthetic
core's synth_wbx.c). docs/trace.md says what a line holds and how it is
built.

Decided without the owner, each easy to change:

- A line for every instruction of both Z80s, the disk unit's too, marked
  `Main` or `Sub`: a PC-8801 game's disk loading runs on the second CPU.
- The instruction written by QUASI88's own disassembler (its debugging
  monitor's), so the core carries no second one.
- In both builds, so the trace does not need the Lua Memory Events setting.
- An 8 MiB buffer a frame, about four times the most measured.

`patches/0004-trace-hooks.patch` adds the call, compiles the disassembler
without the monitor, and adds a read with no side effects for the main CPU.
Two patches now change the same file (z80.c), which `git apply` given the
whole series at once refuses, so `waterbox/apply-patches.sh` applies them one
at a time and tells an already-patched tree by building the patched tree in
a scratch index.

Measured: the gate's three trace tests (docs/trace.md); the traced run equals
the untraced one also with CPU timing 2 and at 8 MHz with memory waits, over
300 traced frames each, with no lines dropped. No cost could be measured
with the trace off (1781 frames a second against 1768 before).
