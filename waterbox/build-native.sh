#!/bin/sh
# build-native.sh - the native reference: QUASI88 and the driver built for
# this host, no sandbox. It is where the core is debugged (gdb, printf), and
# what the gate compares the sandboxed core against.
#
# Usage: build-native.sh
# Output: build/native/run-native
#   Q88_OUT=dir      build somewhere else
#   MINIBOX_DIR=dir  the miniBox checkout (default ../chimera-main/extern/chimera-common-minibox)
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
Q=$root/extern/QUASI88
PORT=$here/port
HERE=$here
MB=${MINIBOX_DIR:-$root/../chimera-main/extern/chimera-common-minibox}
[ -d "$MB/source/guest/include" ] || { echo "no miniBox at $MB (set MINIBOX_DIR)" >&2; exit 1; }
sh "$here/apply-patches.sh" || exit 1
python3 "$root/tools/gen-config.py" >/dev/null || exit 1
. "$here/q88-flags.sh"
out=${Q88_OUT:-$root/build/native}
obj=$out/obj
mkdir -p "$obj"

# The guest kit's headers (settings, slots, ECL_EXPORT) work natively too.
COMMON="-O2 -g -fno-strict-aliasing $Q88_DEFINES $Q88_INCLUDES -I$MB/extern/emulibc -I$MB/source/guest/include -I$MB/extern/jsmn"
SOURCES="$Q88_SOURCES $here/run-native.c"
objs=""
: > "$out/errors.log"
: > "$out/todo"
newest_h=$(ls -t "$here"/*.h "$PORT"/*.h "$here/q88-flags.sh" | head -1)
for s in $SOURCES; do
  rel=${s#$root/}
  o=$obj/$(printf '%s' "$rel" | tr '/' '_' | sed 's/\.[^.]*$/.o/')
  objs="$objs $o"
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ "$newest_h" -nt "$o" ]; then
    rm -f "$o"
    echo "$s $o" >> "$out/todo"
  fi
done
Q88_CC="gcc $COMMON $Q88_CFLAGS" Q88_CXX="g++ $COMMON $Q88_CXXFLAGS" Q88_FLAGS_FILE="$here/q88-flags.sh" Q88_LOG="$out/errors.log" \
  sh "$here/compile-all.sh" < "$out/todo" || { echo "compile failed; see $out/errors.log"; exit 1; }
# malloc is calloc here, as fresh guest memory is zero: a read of memory the
# emulator allocated and never set cannot make the two builds differ
gcc -O2 -c "$here/port/native-malloc.c" -o "$out/native-malloc.o" || exit 1
g++ -o "$out/run-native" $objs "$out/native-malloc.o" -Wl,--wrap=malloc -lm || { echo "link failed"; exit 1; }
echo "OK: $out/run-native"
