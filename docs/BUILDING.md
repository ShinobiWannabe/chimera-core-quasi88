# Building the QUASI88 core

## What you need

- Linux x86_64 (or WSL) with gcc, g++, python3, git, meson and ninja. The
  guest toolchain's C++ library is built from the GCC source that matches
  your gcc, which miniBox downloads once.
- A Chimera checkout, with its submodules, beside this one
  (`../chimera-main`), for miniBox: the sandbox's host library and the guest
  toolchain. Set `MINIBOX_DIR` to use another.

## The sources

    git clone --recursive https://github.com/ShinobiWannabe/chimera-core-quasi88
    cd chimera-core-quasi88

QUASI88 is the submodule `extern/QUASI88`, pinned to one commit of
https://github.com/bubio/QUASI88. It is never edited in place: the changes
are the patches in `patches/`, which `waterbox/apply-patches.sh` applies (all
of them, or none, if they are already there). Every build script runs it.

## miniBox

Build miniBox's host library and its C++ guest toolchain once:

    mb=../chimera-main/extern/chimera-common-minibox
    meson setup $mb/build/meson-linux $mb && meson compile -C $mb/build/meson-linux
    meson setup $mb/build/meson-cpp $mb -Dguest_cpp=true && meson compile -C $mb/build/meson-cpp

To keep the Chimera checkout untouched, build them anywhere else and point
the scripts there with `MINIBOX_BUILD` (the `meson-cpp` folder) and
`MINIBOX_HOST` (the `libminiboxhost.a` in the `meson-linux` folder).

## The builds

| Script | What it makes |
|---|---|
| `waterbox/build-native.sh [w]` | `build/native/run-native`: the emulator and the driver for this computer, no sandbox. The reference the gate compares against. With `w`, the watch build (Lua memory events) in `build/native-w`. |
| `waterbox/build-guest.sh` | `build/wbx/core.wbx`: the same sources for the sandbox, built twice (plain and with the calls at memory, docs/memory-events.md) and linked as one image under `q88-entry.c`, checked by miniBox's `check-wbx.sh`. |
| `waterbox/build-run-wbx.sh` | `build/run-wbx`: runs `core.wbx` through the miniBox host, as Chimera does. |
| `waterbox/build-package.sh` | `build/package/quasi88-<id>.chimeraCore`: the package. With `-r <chimera>` it is also copied to `<chimera>/build/Cores`. |

`tools/gen-config.py` writes the declarations (`waterbox/waterbox.config`,
`default_keybinds.json`, `file_slots.json`, `languages/en.json`) and the
driver's table of controls (`waterbox/q88-input.h`) from one list. The build
scripts run it; commit what it writes.

## Running a game without Chimera

Both harnesses run in a folder laid out as Chimera mounts a project: the
firmware under its ids (`pc88.rom`, `kanji1.rom`, `kanji2.rom`), the disk
images, a `slots` file (`{"floppy": ["Game.d88"]}`) and a `settings` file
(`{}`, or settings such as `{"basicMode": "V1S"}`).

    build/native/run-native <folder> 600 out.ppm
    build/run-wbx build/wbx/core.wbx <folder> 600 out.ppm

Each prints a line a frame: the frame, the picture's digest and size, the
sound samples and their digest, the main CPU's states since power-on, whether
the machine read its controls, and the drive lights. `Q88_PRESS="900:Key
SPACE:10"` holds a control from a frame for some frames (press-script.h);
run-native.c and run-wbx.c list the other variables.

## The gate

    Q88_FIRMWARE=<folder with PC88.ROM and KANJI1.ROM> Q88_GAMES=<folder with the test games> \
      waterbox/run-gate.sh

It builds everything and compares the native reference with the sandboxed
core on named games, with and without input, around a savestate, with the
host stalled and in turbo, with Lua memory events and the trace logger. Without firmware and games the machine tests say
SKIP. Chimera's contract tests run against the package:

    <chimera>/build/meson-linux/source/ui/test-packages build/package

## Troubleshooting

- `no miniBox C++ guest sysroot`: build miniBox's `meson-cpp` (above), or set
  `MINIBOX_BUILD`.
- `extern/QUASI88 is neither clean nor patched`: the submodule has changes of
  its own; `git -C extern/QUASI88 checkout .` and build again.
- A compile error is in `build/native/errors.log` or `build/wbx/errors.log`.
