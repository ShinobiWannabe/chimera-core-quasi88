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
	sh "$here/build-native.sh" w >/dev/null && ok "native reference builds with the calls at memory" || { bad "native watch build"; exit 1; }
	sh "$here/build-guest.sh" >/dev/null && ok "core.wbx builds (both halves) and check-wbx.sh finds it clean" || { bad "core.wbx builds"; exit 1; }
	sh "$here/build-run-wbx.sh" >/dev/null && ok "run-wbx builds" || { bad "run-wbx builds"; exit 1; }
fi
native=$build/native/run-native
nativew=$build/native-w/run-native
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

# ---- firmware: the sizes Init takes and refuses ----
# PC88.ROM comes as 88 KiB, or 112 KiB with the whole of N-BASIC; the
# declarations take any size, so the driver's Init is what checks. With an
# 88 KiB one the N mode takes N-BASIC from N88N.ROM, which must end in the
# 8 KiB PC88.ROM holds at 8000h: one made to match starts, one that does
# not gives error.nbasic_mismatch.
fwsize() {	# name settings-json -> the Init line of run-native, or "ok"
	printf '%s' "$2" > "$work/fw/settings"
	if $native "$work/fw" 1 > /dev/null 2> "$work/fw.err"; then echo ok; else grep -o 'error\.[a-z_]*' "$work/fw.err" | head -1; fi
}
mkdir -p "$work/fw"; rm -f "$work/fw"/*
cp "$kanji1" "$work/fw/kanji1.rom"; printf '{}' > "$work/fw/slots"
len=$(wc -c < "$pc88")
if [ "$len" -eq 90112 ] || [ "$len" -eq 114688 ]; then
	head -c 90112 "$pc88" > "$work/fw/pc88.rom"
	r1=$(fwsize pc88.rom '{"bootFrom":"ROM"}'); r2=$(fwsize pc88.rom '{"bootFrom":"ROM","basicMode":"N"}')
	{ head -c 24576 /dev/zero; dd if="$pc88" bs=8192 skip=4 count=1 2>/dev/null; } > "$work/fw/n88n.rom"
	r5=$(fwsize pc88.rom '{"bootFrom":"ROM","basicMode":"N"}')
	head -c 32768 /dev/zero > "$work/fw/n88n.rom"
	r6=$(fwsize pc88.rom '{"bootFrom":"ROM","basicMode":"N"}')
	rm -f "$work/fw/n88n.rom"
	head -c 80000 "$pc88" > "$work/fw/pc88.rom"
	r3=$(fwsize pc88.rom '{"bootFrom":"ROM"}')
	{ head -c 90112 "$pc88"; head -c 24576 /dev/zero | tr '\000' '\377'; } > "$work/fw/pc88.rom"
	[ "$len" -eq 114688 ] && cp "$pc88" "$work/fw/pc88.rom"
	r4=$(fwsize pc88.rom '{"bootFrom":"ROM","basicMode":"N"}')
	if [ "$r1" = ok ] && [ "$r2" = error.no_nbasic ] && [ "$r5" = ok ] && [ "$r6" = error.nbasic_mismatch ] && \
	   [ "$r3" = error.bad_firmware_size ] && [ "$r4" = ok ]; then
		ok "firmware: 88 KiB PC88.ROM starts; its N mode needs a matching N88N.ROM; 112 KiB starts N mode; 80000 bytes is refused"
	else bad "firmware sizes: 88K=$r1, 88K N=$r2, 88K N + matching N88N=$r5, + other N88N=$r6, 80000=$r3, 112K N=$r4"; fi
else
	skp "firmware sizes: PC88.ROM is $len bytes, neither 90112 nor 114688"
fi

# ---- drives: Boot From = ROM survives a swap into drive 1 and Reset ----
# Two disks, the machine started in BASIC. A Reset alone and a Reset after
# swapping the other disk into drive 1 must both start BASIC again: the same
# picture from then on. Upstream turns a drive 1 insert into a disk boot
# (control.c), which the driver undoes (machine_reset).
g1=$(game Master_of_Monsters.D88); g2=$(game DIGDUG_SR.D88)
if [ -n "$g1" ] && [ -n "$g2" ]; then
	setup t "$g1" '{"bootFrom":"ROM"}'
	cp "$g2" "$work/t/"
	printf '{"floppy": ["%s", "%s"]}' "$(basename "$g1")" "$(basename "$g2")" > "$work/t/slots"
	Q88_PRESS="400:Reset:2" $native "$work/t" 900 > "$work/a.log" 2>/dev/null
	Q88_PRESS="300:Drive 1 Next Floppy Disk:2,320:Drive 1 Swap Floppy Disk:2,400:Reset:2" $native "$work/t" 900 > "$work/b.log" 2>/dev/null
	Q88_PRESS="300:Drive 1 Next Floppy Disk:2,320:Drive 1 Swap Floppy Disk:2,400:Reset:2" $wbx "$work/t" 900 2>/dev/null | grep '^[0-9]' > "$work/w.log"
	if [ "$(awk '$1 >= 700 {print $2}' "$work/a.log")" = "$(awk '$1 >= 700 {print $2}' "$work/b.log")" ] && \
	   [ -n "$(awk '$1 >= 700' "$work/a.log")" ] && cmp -s "$work/b.log" "$work/w.log"; then
		ok "drives: after a drive 1 swap, Reset still boots from ROM (picture as a plain Reset); native = sandbox"
	else bad "drives: a drive 1 swap then Reset did not boot as Boot From = ROM says, or native and sandbox differ"; fi
else
	skp "drives: Master_of_Monsters.D88 and DIGDUG_SR.D88 not both in Q88_GAMES"
fi

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
	if awk '$7 == 0' "$work/n.log" | grep -q . && awk '$7 == 1' "$work/n.log" | grep -q .; then
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

# ---- Lua memory events: the watch half (q88-entry.c, port/memwatch.c) ----
path=$(game Xevious.d88)
if [ -n "$path" ]; then
	setup t "$path" '{"basicMode":"V1S"}'
	$native "$work/t" 900 > "$work/p.log" 2>/dev/null
	setup tw "$path" '{"basicMode":"V1S","luaMemoryEvents":true}'
	$nativew "$work/tw" 900 > "$work/a.log" 2>/dev/null
	$wbx "$work/tw" 900 2>/dev/null | grep '^[0-9]' > "$work/b.log"
	if cmp -s "$work/p.log" "$work/a.log" && cmp -s "$work/p.log" "$work/b.log"; then
		ok "memory events: the watch build with nothing watched runs the plain build's machine (native and sandbox)"
	else bad "memory events: the watch build differs from the plain one"; fi
	W="x:any,r:any,w:any,x1:any,r1:any,w1:any"
	Q88_WATCH=$W Q88_WATCH_MAX=0 $nativew "$work/tw" 900 > "$work/a.log" 2>/dev/null
	Q88_WATCH=$W Q88_WATCH_MAX=0 $wbx "$work/tw" 900 > "$work/b.log" 2>/dev/null
	ca=$(grep -A2 '^WATCH scopes' "$work/a.log" | grep -o 'x=.*'); cb=$(grep -A2 '^WATCH scopes' "$work/b.log" | grep -o 'x=.*')
	if [ -n "$ca" ] && [ "$ca" = "$cb" ] && grep '^[0-9]' "$work/a.log" | cmp -s - "$work/p.log" && grep '^[0-9]' "$work/b.log" | cmp -s - "$work/p.log"; then
		ok "memory events: every address of both CPUs watched, the machine is unchanged and native = sandbox ($ca)"
	else bad "memory events: watching changed the machine, or native and sandbox called differently ($ca / $cb)"; fi
	# the control: a script's replacement value reaches the machine
	Q88_WATCH="w:any=0" Q88_WATCH_MAX=0 $nativew "$work/tw" 900 > "$work/c.log" 2>/dev/null
	if grep '^[0-9]' "$work/c.log" | cmp -s - "$work/p.log"; then bad "memory events: a replaced write changed nothing"
	else ok "memory events: every write replaced with 0 changes the machine, as a script's value should"; fi
	r=$(Q88_WATCH="r:any" $wbx "$work/t" 10 2>&1 | grep -c "would not watch")
	[ "$r" -ge 1 ] && ok "memory events: with the setting off the core refuses watches" || bad "memory events: the plain half took a watch"
else
	skp "memory events: Xevious.d88 not in Q88_GAMES"
fi

# ---- the trace logger (port/trace.c, patches/0004) ----
# Traced from frame 400 to 449 with memory waits on, where a real read of an
# instruction's bytes would cost the machine time: the trace must leave the
# run as it was, be the same from native and sandbox, cover both CPUs, and
# be the same from the watch half.
path=$(game Xevious.d88)
if [ -n "$path" ]; then
	setup t "$path" '{"basicMode":"V1S","memoryWait":true}'
	$native "$work/t" 450 /dev/null "$work/p.mem" > "$work/p.log" 2>/dev/null
	Q88_TRACE=$work/n.trace,400 $native "$work/t" 450 /dev/null "$work/n.mem" > "$work/n.log" 2>"$work/n.err"
	Q88_TRACE=$work/w.trace,400 $wbx "$work/t" 450 /dev/null "$work/w.mem" 2>"$work/w.err" | grep '^[0-9]' > "$work/w.log"
	tn=$(grep '^TRACE' "$work/n.err"); tw=$(grep '^TRACE' "$work/w.err")
	if cmp -s "$work/p.log" "$work/n.log" && cmp -s "$work/p.mem" "$work/n.mem" && cmp -s "$work/n.log" "$work/w.log" && cmp -s "$work/n.mem" "$work/w.mem"; then
		ok "trace: 50 frames traced run the untraced machine (picture, sound, clock, RAM), native and sandbox"
	else bad "trace: tracing changed the machine, or native and sandbox differ"; fi
	nm=$(grep -c '^Main ' "$work/n.trace"); ns=$(grep -c '^Sub ' "$work/n.trace")
	odd=$(grep -cvE '^(Main|Sub) [0-9A-F]{4}: [0-9A-F]+ +[A-Z].*	AF:[0-9A-F]{4} BC:[0-9A-F]{4} DE:[0-9A-F]{4} HL:[0-9A-F]{4} IX:[0-9A-F]{4} IY:[0-9A-F]{4} SP:[0-9A-F]{4}$' "$work/n.trace")
	case "$tn" in *", 0 overflowed") dropped=0 ;; *) dropped=1 ;; esac
	if [ -n "$tn" ] && [ "$tn" = "$tw" ] && cmp -s "$work/n.trace" "$work/w.trace" && [ "$nm" -gt 0 ] && [ "$ns" -gt 0 ] && [ "$odd" -eq 0 ] && [ $dropped -eq 0 ]; then
		ok "trace: native = sandbox, line for line ($nm main CPU and $ns sub CPU lines, none dropped, every line well formed)"
	else bad "trace: the lines differ or are malformed ($tn / $tw; $nm main, $ns sub, $odd malformed)"; fi
	setup tw "$path" '{"basicMode":"V1S","memoryWait":true,"luaMemoryEvents":true}'
	Q88_TRACE=$work/ww.trace,400 $wbx "$work/tw" 450 2>/dev/null | grep '^[0-9]' > "$work/ww.log"
	if cmp -s "$work/w.trace" "$work/ww.trace" && cmp -s "$work/w.log" "$work/ww.log"; then
		ok "trace: the watch half (Lua Memory Events on) traces the same lines"
	else bad "trace: the watch half traces differently"; fi
	rm -f "$work"/*.trace
else
	skp "trace: Xevious.d88 not in Q88_GAMES"
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
