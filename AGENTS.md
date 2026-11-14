# AGENTS.md - chimera-core-quasi88

QUASI88 (NEC PC-8801) as a Chimera core. Chimera's own `AGENTS.md` and
`docs/new-core.md` are the rules this repository follows; docs/PLAN.md is the
design log, docs/BUILDING.md the build.

## Layout

- `extern/QUASI88` - upstream, a pinned submodule. Never edited in place.
- `patches/` - numbered changes to upstream, applied by
  `waterbox/apply-patches.sh` (all or none).
- `waterbox/port/` - QUASI88's system layer for the core (screen, sound,
  files, events, the calendar's clock).
- `waterbox/q88-driver.c` - the Chimera ABI: Init, FrameAdvance, picture,
  sound, input, memory, drives, registers, save data.
- `waterbox/q88-flags.sh` - the sources and flags both builds share.
- `tools/gen-config.py` - the one table of controls, settings, firmware and
  texts. It writes `waterbox/q88-input.h`, `waterbox.config`,
  `default_keybinds.json`, `file_slots.json` and `languages/en.json`.
- `build/` - every output, ignored by git.

## Commands, in order

    git submodule update --init
    waterbox/build-native.sh          # build/native/run-native
    waterbox/build-guest.sh           # build/wbx/core.wbx
    waterbox/build-run-wbx.sh         # build/run-wbx
    Q88_FIRMWARE=... Q88_GAMES=... waterbox/run-gate.sh
    waterbox/build-package.sh [-r <chimera>]

`MINIBOX_DIR`, `MINIBOX_BUILD` and `MINIBOX_HOST` say where miniBox and its
builds are when they are not in `../chimera-main`.

## Rules

- A change to a control, setting, firmware entry or text is made in
  `tools/gen-config.py`; commit what it writes with it.
- A change to upstream is a new numbered patch, a hook or a build option.
- Run the gate before a commit that touches what it covers, and say in the
  commit what ran. A new test is broken on purpose once and seen to fail.
- No firmware or game in the repository, in CI or in an issue. Tests that
  need them read `Q88_FIRMWARE` and `Q88_GAMES` and say SKIP without them.
- Write in plain language; prose is ASCII.
- Commits: `type(scope): what changed`, a short body with what was tested,
  explicit paths staged.
