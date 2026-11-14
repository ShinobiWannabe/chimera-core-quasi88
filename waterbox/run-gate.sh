#!/bin/sh
# run-gate.sh - the core gate: the native reference against the sandboxed
# core, test by test, each saying what it compared.
#
# Usage: run-gate.sh
#   MINIBOX_DIR    the miniBox checkout (default ../chimera-main/extern/chimera-common-minibox)
#   MINIBOX_BUILD  its C++ guest build (default $MINIBOX_DIR/build/meson-cpp)
#   MINIBOX_HOST   its libminiboxhost.a, when built outside the checkout
#   Q88_FIRMWARE   a folder holding PC88.ROM (or pc88.rom) and KANJI1.ROM
#   Q88_GAMES      a folder holding the test games named below
#   Q88_SKIP_BUILD=1  use the builds already there
#
# No firmware or game may be distributed, so the machine tests need
# Q88_FIRMWARE and Q88_GAMES and say SKIP, with the reason, without them. A
# SKIP is not a pass. The games are named, never "the first file":
#   Xevious.d88            (BASIC mode V1S) the title, then a game started with
#                          SPACE and played with the joystick
#   Master_of_Monsters.D88 the title with its music
#   DIGDUG_SR.D88          the title
# The build, the image check and the declarations need no content.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
build=$root/build
work=$build/gate
pass=0; fail=0; skip=0
ok() { echo "PASS  $*"; pass=$((pass + 1)); }
bad() { echo "FAIL  $*"; fail=$((fail + 1)); }
skp() { echo "SKIP  $*"; skip=$((skip + 1)); }

# ---- builds ----
if [ -z "${Q88_SKIP_BUILD:-}" ]; then
	sh "$here/build-native.sh" >/dev/null && ok "native reference builds" || { bad "native reference builds"; exit 1; }
	sh "$here/build-guest.sh" >/dev/null && ok "core.wbx builds and check-wbx.sh finds it clean" || { bad "core.wbx builds"; exit 1; }
	sh "$here/build-run-wbx.sh" >/dev/null && ok "run-wbx builds" || { bad "run-wbx builds"; exit 1; }
fi
native=$build/native/run-native
wbx="$build/run-wbx $build/wbx/core.wbx"

# ---- the declarations agree with the driver ----
python3 "$root/tools/gen-config.py" >/dev/null
if git -C "$root" diff --quiet -- waterbox/waterbox.config waterbox/q88-input.h waterbox/default_keybinds.json waterbox/file_slots.json waterbox/languages 2>/dev/null; then
	ok "tools/gen-config.py leaves the committed declarations as they are"
else
	bad "tools/gen-config.py changed the declarations: commit them with the tables"
fi

# ---- content ----
fw=${Q88_FIRMWARE:-}; games=${Q88_GAMES:-}
find_file() { for n in "$@"; do [ -f "$dir/$n" ] && { echo "$dir/$n"; return 0; }; done; return 1; }
pc88=""; kanji1=""
if [ -n "$fw" ] && [ -d "$fw" ]; then
	dir=$fw
	pc88=$(find_file PC88.ROM pc88.rom || true)
	kanji1=$(find_file KANJI1.ROM kanji1.rom N88KNJ1.ROM || true)
fi
if [ -z "$pc88" ] || [ -z "$kanji1" ]; then
	skp "every machine test: no PC88.ROM and KANJI1.ROM (set Q88_FIRMWARE)"
	echo "gate: $pass passed, $fail failed, $skip skipped"
	[ $fail -eq 0 ]; exit $?
fi

# a folder as the engine mounts a project: firmware by id, the disk, slots, settings
setup() {	# name game settings-json
	d=$work/$1
	mkdir -p "$d"
	rm -f "$d"/*
	cp "$pc88" "$d/pc88.rom"; cp "$kanji1" "$d/kanji1.rom"
	cp "$2" "$d/"
	printf '{"floppy": ["%s"]}' "$(basename "$2")" > "$d/slots"
	printf '%s' "$3" > "$d/settings"
}
lines() { grep '^[0-9]' "$1"; }

game() {	# file -> path, or empty
	[ -n "$games" ] && [ -f "$games/$1" ] && echo "$games/$1"
}

# ---- native against the sandbox ----
for spec in "Xevious.d88|1500|{\"basicMode\":\"V1S\"}" "Master_of_Monsters.D88|1800|{}" "DIGDUG_SR.D88|1200|{}" \
            "Master_of_Monsters.D88|1800|{\"soundBoard\":\"Sound Board II\",\"soundGenerator\":\"fmgen\"}"; do
	g=${spec%%|*}; rest=${spec#*|}; n=${rest%%|*}; set=${rest#*|}
	path=$(game "$g")
	[ -n "$path" ] || { skp "native = sandbox, $g: not in Q88_GAMES"; continue; }
	setup t "$path" "$set"
	$native "$work/t" "$n" /dev/null "$work/n.mem" > "$work/n.log" 2>/dev/null
	$wbx "$work/t" "$n" /dev/null "$work/w.mem" 2>/dev/null | grep '^[0-9]' > "$work/w.log"
	if [ "$(wc -l < "$work/n.log")" -eq "$n" ] && cmp -s "$work/n.log" "$work/w.log" && cmp -s "$work/n.mem" "$work/w.mem"; then
		ok "native = sandbox, $g $set: $n frames of picture, sound, clock and lights, and the main RAM after"
	else
		bad "native = sandbox, $g $set: $(cmp "$work/n.log" "$work/w.log" 2>&1 | head -1)"
	fi
done

# ---- input: a game started and played, through SetButton ----
path=$(game Xevious.d88)
if [ -n "$path" ]; then
	setup t "$path" '{"basicMode":"V1S"}'
	$native "$work/t" 1800 > "$work/still.log" 2>/dev/null
	Q88_PRESS="900:Key SPACE:10,1300:P1 Joystick Up:60,1360:P1 Joystick Left:30" $native "$work/t" 1800 /dev/null "$work/n.mem" > "$work/n.log" 2>/dev/null
	Q88_PRESS="900:Key SPACE:10,1300:P1 Joystick Up:60,1360:P1 Joystick Left:30" $wbx "$work/t" 1800 /dev/null "$work/w.mem" 2>/dev/null | grep '^[0-9]' > "$work/w.log"
	if cmp -s "$work/n.log" "$work/still.log"; then bad "input: SPACE and the joystick changed nothing"
	elif cmp -s "$work/n.log" "$work/w.log" && cmp -s "$work/n.mem" "$work/w.mem"; then ok "input: SPACE starts Xevious and the joystick moves; native = sandbox"
	else bad "input: native and sandbox differ: $(cmp "$work/n.log" "$work/w.log" 2>&1 | head -1)"; fi
	# lag frames: frames on which the machine read no control
	if awk '$8 == 0' "$work/n.log" | grep -q . && awk '$8 == 1' "$work/n.log" | grep -q .; then
		ok "lag frames: some frames read the controls and some did not"
	else
		bad "lag frames: every frame says the same"
	fi
	# savestates: saved before frame 1100 in play, loaded, played again
	r=$(Q88_PRESS="900:Key SPACE:10,1300:P1 Joystick Up:60" Q88_STATE_AT=1100 $wbx "$work/t" 1800 2>/dev/null | grep STATE)
	case "$r" in "STATE same"*) ok "savestate: loaded in play, the run is the straight one ($r)" ;; *) bad "savestate: $r" ;; esac
	# its negative control: one extra SPACE on the title after the load must show
	r=$(Q88_STATE_AT=1000 Q88_STATE_CONTROL="Key SPACE" $wbx "$work/t" 1800 2>/dev/null | grep STATE)
	case "$r" in "STATE DIFFER"*) ok "savestate control: an extra press after the load is seen" ;; *) bad "savestate control: an extra press was not seen ($r)" ;; esac
else
	skp "input, lag frames and savestates: Xevious.d88 not in Q88_GAMES"
fi

# ---- time: the host stalling does not move the machine ----
path=$(game Master_of_Monsters.D88)
if [ -n "$path" ]; then
	setup t "$path" '{}'
	$native "$work/t" 900 > "$work/a.log" 2>/dev/null
	Q88_STALL_FRAME=300 $native "$work/t" 900 > "$work/b.log" 2>/dev/null
	cmp -s "$work/a.log" "$work/b.log" && ok "time: a 300 ms stall of the host before frame 300 changes nothing" || bad "time: the stall moved the machine"
	# turbo: frames not copied out leave the machine as it was
	$wbx "$work/t" 900 2>/dev/null | grep '^[0-9]' | cut -d' ' -f1,3- > "$work/a.log"
	Q88_NORENDER=200,400 $wbx "$work/t" 900 2>/dev/null | grep '^[0-9]' | cut -d' ' -f1,3- > "$work/b.log"
	cmp -s "$work/a.log" "$work/b.log" && ok "turbo: frames 200-399 not copied out change no sound, clock or light" || bad "turbo changed the machine"
else
	skp "time and turbo: Master_of_Monsters.D88 not in Q88_GAMES"
fi

# ---- tapes: a program saved to tape from BASIC, then loaded and run ----
# Needs only the firmware: the machine starts in BASIC with no disk.
d=$work/tape
mkdir -p "$d" "$work/saved"
rm -f "$d"/* "$work/saved"/*
cp "$pc88" "$d/pc88.rom"; cp "$kanji1" "$d/kanji1.rom"
printf '{}' > "$d/slots"; printf '{"bootFrom":"ROM"}' > "$d/settings"
P=$(python3 "$root/tools/type-keys.py" 400 '|10 PRINT 12345|SAVE ~CAS:A~|' 2>/dev/null)
Q88_SAVEDATA=$work/saved Q88_PRESS="$P" $native "$d" 1500 > /dev/null 2>&1
if [ -s "$work/saved/tape-save.cmt" ]; then
	ok "tape save: SAVE \"CAS:A\" from BASIC leaves tape-save.cmt as save data ($(wc -c < "$work/saved/tape-save.cmt") bytes)"
	cp "$work/saved/tape-save.cmt" "$d/test.cmt"
	printf '{"tape": ["test.cmt"]}' > "$d/slots"
	P=$(python3 "$root/tools/type-keys.py" 400 '|LOAD ~CAS:A~|RUN|' 2>/dev/null)
	Q88_PRESS="$P" $native "$d" 1500 /dev/null "$work/n.mem" > "$work/n.log" 2>/dev/null
	Q88_PRESS="$P" $wbx "$d" 1500 /dev/null "$work/w.mem" 2>/dev/null | grep '^[0-9]' > "$work/w.log"
	# the control: the same typing with no tape in the recorder
	printf '{}' > "$d/slots"
	Q88_PRESS="$P" $native "$d" 1500 /dev/null "$work/c.mem" > /dev/null 2>&1
	if cmp -s "$work/n.mem" "$work/c.mem"; then bad "tape load: RAM is the same with no tape in"
	elif cmp -s "$work/n.log" "$work/w.log" && cmp -s "$work/n.mem" "$work/w.mem"; then ok "tape load: LOAD \"CAS:A\" reads the tape back; native = sandbox"
	else bad "tape load: native and sandbox differ"; fi
else
	bad "tape save: no tape-save.cmt after SAVE \"CAS:A\""
fi

echo "gate: $pass passed, $fail failed, $skip skipped"
[ $fail -eq 0 ]
