#!/bin/sh
# compile-all.sh - compiles a list of sources in parallel, for build-native.sh
# and build-guest.sh. Reads "source object" pairs on stdin; the compilers
# and flags come in the environment:
#   Q88_CC, Q88_CXX   the C and C++ compile commands, flags included
#   Q88_FLAGS_FILE    a shell file defining q88_file_flags (q88-flags.sh)
#   Q88_LOG           where the compilers' messages go
# Exits 1 when any source failed, naming it.
set -u
jobs=${Q88_JOBS:-$(nproc 2>/dev/null || echo 4)}
xargs -n 2 -P "$jobs" sh -c '
	. "$Q88_FLAGS_FILE"
	s=$1; o=$2
	case "$s" in
		*.cpp) $Q88_CXX $(q88_file_flags "$s") -w -c "$s" -o "$o" 2>"$o.log" ;;
		*)     $Q88_CC $(q88_file_flags "$s") -w -c "$s" -o "$o" 2>"$o.log" ;;
	esac
	r=$?
	cat "$o.log" >> "$Q88_LOG"; rm -f "$o.log"
	[ $r -eq 0 ] || { echo "FAILED: $s"; exit 255; }
' sh
