#!/bin/sh
# build-guest.sh - the core itself: QUASI88 and the driver built for the
# miniBox guest (musl, -mcmodel=large, fixed base), as core.wbx.
#
# The same sources and the same q88-flags.sh as build-native.sh, so the two
# builds differ only in the C library and the code model.
#
# Usage: build-guest.sh
# Output: build/wbx/core.wbx
#   MINIBOX_DIR=dir    the miniBox checkout (default ../chimera-main/extern/chimera-common-minibox)
#   MINIBOX_BUILD=dir  its C++ guest build (default $MINIBOX_DIR/build/meson-cpp)
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
Q=$root/extern/QUASI88
PORT=$here/port
HERE=$here
MB=${MINIBOX_DIR:-$root/../chimera-main/extern/chimera-common-minibox}
MBB=${MINIBOX_BUILD:-$MB/build/meson-cpp}
SR=$MBB/guest-sysroot
[ -f "$SR/lib/musl-gcc.specs" ] || { echo "no miniBox C++ guest sysroot at $SR (meson -Dguest_cpp=true, then meson compile)" >&2; exit 1; }
sh "$here/apply-patches.sh" || exit 1
python3 "$root/tools/gen-config.py" >/dev/null || exit 1
. "$here/q88-flags.sh"
out=${Q88_OUT:-$root/build/wbx}
obj=$out/obj
mkdir -p "$obj"

# The guest flags, as miniBox's own guest build and the NP2kai core have
# them. -mno-red-zone: see miniBox docs/RED-ZONE.md. The thread_local
# defines make any thread-local variable a plain one: the guest has one
# thread.
GCCVER=$(gcc -dumpfullversion)
GUEST="-specs $SR/lib/musl-gcc.specs -fvisibility=hidden -mcmodel=large -mno-red-zone -mno-tls-direct-seg-refs \
 -mstack-protector-guard=global -fno-stack-protector -fno-pic -fno-pie \
 -fcf-protection=none -DCHIMERA_GUEST -Dthread_local= -D_Thread_local= -D__thread= \
 -I$MB/extern/emulibc -I$MB/source/guest/include -I$MB/extern/jsmn"
GUESTCXX="$GUEST -fexceptions -I$SR/include/c++/$GCCVER -I$SR/include/c++/$GCCVER/x86_64-linux-musl"

COMMON="-O2 -fno-strict-aliasing $Q88_DEFINES $Q88_INCLUDES"
objs=""
: > "$out/errors.log"
: > "$out/todo"
newest_h=$(ls -t "$here"/*.h "$PORT"/*.h "$here/q88-flags.sh" | head -1)
for s in $Q88_SOURCES; do
  rel=${s#$root/}
  o=$obj/$(printf '%s' "$rel" | tr '/' '_' | sed 's/\.[^.]*$/.o/')
  objs="$objs $o"
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ "$newest_h" -nt "$o" ]; then
    rm -f "$o"
    echo "$s $o" >> "$out/todo"
  fi
done
Q88_CC="gcc $GUEST $COMMON $Q88_CFLAGS" Q88_CXX="g++ $GUESTCXX $COMMON $Q88_CXXFLAGS" Q88_FLAGS_FILE="$here/q88-flags.sh" Q88_LOG="$out/errors.log" \
  sh "$here/compile-all.sh" < "$out/todo" || { echo "compile failed; see $out/errors.log"; exit 1; }

# miniBox's C++ guest link recipe (source/guest/meson.build). The order is
# load-bearing: -lstdc++ -lgcc -lgcc_eh -lc, libc last; --no-relax for the
# large code model; the -u pthread_* because libgcc_eh refers to them weakly.
g++ $GUESTCXX -o "$out/core.wbx" $objs \
   -static -no-pie -Wl,--eh-frame-hdr,-O2,--no-relax \
   -T "$MB/source/guest/linkscript.T" \
   -Wl,-u,pthread_once -Wl,-u,pthread_cond_wait \
   -Wl,-u,pthread_cond_broadcast -Wl,-u,pthread_key_create \
   "$MBB/source/guest/cxxglue.c.o" "$MBB/source/guest/emulibc.c.o" \
   -L"$SR/lib" -lstdc++ -lgcc -lgcc_eh -lc 2>>"$out/errors.log" || { echo "link failed; see $out/errors.log"; exit 1; }
echo "OK: $out/core.wbx"
# miniBox's guest link recipe ends with wbx-tp-rewrite.py on the image: on
# x86-64 it lists where the code loads the thread pointer, for an Intel Mac.
if [ -f "$MB/source/guest/wbx-tp-rewrite.py" ]; then
  python3 "$MB/source/guest/wbx-tp-rewrite.py" "$out/core.wbx" || { echo "wbx-tp-rewrite.py failed"; exit 1; }
fi
sh "$MB/source/guest/check-wbx.sh" "$out/core.wbx" || { echo "check-wbx.sh reported problems"; exit 1; }
