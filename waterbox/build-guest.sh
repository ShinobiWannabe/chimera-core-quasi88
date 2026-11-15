#!/bin/sh
# build-guest.sh - the core itself: QUASI88 and the driver built for the
# miniBox guest (musl, -mcmodel=large, fixed base), as core.wbx.
#
# The same sources and the same q88-flags.sh as build-native.sh, so the
# builds differ only in the C library and the code model.
#
# core.wbx holds QUASI88 twice (docs/memory-events.md): the plain build and
# the watch build (the calls at memory), each linked into one relocatable
# half with its driver's exports renamed q88_* and q88w_* and every other
# symbol made local, and q88-entry.c on top to pick the half the Lua Memory
# Events setting names.
#
# Usage: build-guest.sh
# Output: build/wbx/core.wbx (the halves in build/wbx-plain and build/wbx-w)
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
out=$root/build/wbx
mkdir -p "$out"

# The guest flags, as miniBox's own guest build and the NP2kai core have
# them. -mno-red-zone: see miniBox docs/RED-ZONE.md. The thread_local
# defines make any thread-local variable a plain one: the guest has one
# thread. -fno-gnu-unique makes C++'s one-per-program statics weak, which a
# half can make local.
GCCVER=$(gcc -dumpfullversion)
GUEST="-specs $SR/lib/musl-gcc.specs -fvisibility=hidden -mcmodel=large -mno-red-zone -mno-tls-direct-seg-refs \
 -mstack-protector-guard=global -fno-stack-protector -fno-pic -fno-pie \
 -fcf-protection=none -DCHIMERA_GUEST -Dthread_local= -D_Thread_local= -D__thread= \
 -I$MB/extern/emulibc -I$MB/source/guest/include -I$MB/extern/jsmn"
GUESTCXX="$GUEST -fexceptions -fno-gnu-unique -I$SR/include/c++/$GCCVER -I$SR/include/c++/$GCCVER/x86_64-linux-musl"

# the driver's exports, as the table names them (q88-exports.h), and Init
exports() {
	sed -n 's/^[[:space:]]*X(\([^,]*\), *\([A-Za-z0-9_]*\),.*/\2/p; s/^[[:space:]]*X(\([A-Za-z0-9_]*\), (.*/\1/p' "$here/q88-exports.h" | sort -u
	echo Init
}

# one half: every source compiled with the flavour's flags, linked into one
# relocatable object with the exports renamed <prefix>_<name>
half() {	# flavour ("" or w) prefix
	Q88_FLAVOUR=$1
	export Q88_FLAVOUR
	. "$here/q88-flags.sh"
	hout=$root/build/wbx-${1:-plain}
	obj=$hout/obj
	mkdir -p "$obj"
	COMMON="-O2 -fno-strict-aliasing $Q88_DEFINES $Q88_INCLUDES"
	objs=""
	: > "$hout/errors.log"
	: > "$hout/todo"
	newest_h=$(ls -t "$here"/*.h "$PORT"/*.h "$here/q88-flags.sh" | head -1)
	for s in $Q88_SOURCES; do
		rel=${s#$root/}
		o=$obj/$(printf '%s' "$rel" | tr '/' '_' | sed 's/\.[^.]*$/.o/')
		objs="$objs $o"
		if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ "$newest_h" -nt "$o" ]; then
			rm -f "$o"
			echo "$s $o" >> "$hout/todo"
		fi
	done
	Q88_CC="gcc $GUEST $COMMON $Q88_CFLAGS" Q88_CXX="g++ $GUESTCXX $COMMON $Q88_CXXFLAGS" Q88_FLAGS_FILE="$here/q88-flags.sh" Q88_LOG="$hout/errors.log" \
		sh "$here/compile-all.sh" < "$hout/todo" || { echo "compile failed; see $hout/errors.log"; return 1; }
	# C++ inline code and templates come in COMDAT groups, which the final
	# link keeps once by name for the whole program; removing the groups
	# keeps each half's own copy.
	# -d gives the common symbols (-fcommon) their space now, so they can be
	# made local like the rest
	ld -r -d -o "$hout/half-raw.o" $objs 2>>"$hout/errors.log" || { echo "ld -r failed; see $hout/errors.log"; return 1; }
	exports | sed "s/.*/& ${2}_&/" > "$hout/rename"
	objcopy -w --redefine-syms="$hout/rename" "$hout/half-raw.o" "$hout/half-renamed.o" || return 1
	objcopy -w --keep-global-symbol="${2}_*" -R .group "$hout/half-renamed.o" "$hout/half.o" || return 1
	echo "OK: $hout/half.o"
}

half "" q88 || exit 1
half w q88w || exit 1

# A name both halves still define would be one function for two machines:
# only the renamed exports may be global, and those differ by prefix.
dup=$(for h in plain w; do nm -g --defined-only "$root/build/wbx-$h/half.o" | awk '{print $3}'; done | sort | uniq -d)
[ -z "$dup" ] || { echo "both halves define:"; echo "$dup"; exit 1; }

gcc $GUEST -O2 -I"$here" -c "$here/q88-entry.c" -o "$out/q88-entry.o" || exit 1
# miniBox's C++ guest link recipe (source/guest/meson.build). The order is
# load-bearing: -lstdc++ -lgcc -lgcc_eh -lc, libc last; --no-relax for the
# large code model; the -u pthread_* because libgcc_eh refers to them weakly.
g++ $GUESTCXX -o "$out/core.wbx" "$out/q88-entry.o" "$root/build/wbx-plain/half.o" "$root/build/wbx-w/half.o" \
   -static -no-pie -Wl,--eh-frame-hdr,-O2,--no-relax \
   -T "$MB/source/guest/linkscript.T" \
   -Wl,-u,pthread_once -Wl,-u,pthread_cond_wait \
   -Wl,-u,pthread_cond_broadcast -Wl,-u,pthread_key_create \
   "$MBB/source/guest/cxxglue.c.o" "$MBB/source/guest/emulibc.c.o" \
   -L"$SR/lib" -lstdc++ -lgcc -lgcc_eh -lc 2>"$out/errors.log" || { echo "link failed; see $out/errors.log"; exit 1; }
echo "OK: $out/core.wbx"
# miniBox's guest link recipe ends with wbx-tp-rewrite.py on the image: on
# x86-64 it lists where the code loads the thread pointer, for an Intel Mac.
if [ -f "$MB/source/guest/wbx-tp-rewrite.py" ]; then
  python3 "$MB/source/guest/wbx-tp-rewrite.py" "$out/core.wbx" || { echo "wbx-tp-rewrite.py failed"; exit 1; }
fi
sh "$MB/source/guest/check-wbx.sh" "$out/core.wbx" || { echo "check-wbx.sh reported problems"; exit 1; }
