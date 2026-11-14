#!/bin/sh
# build-run-wbx.sh - builds run-wbx, the harness that drives core.wbx through
# the miniBox host directly: no frontend, no package.
#   MINIBOX_DIR=dir    the miniBox checkout
#   MINIBOX_HOST=file  its libminiboxhost.a, when it is built outside the checkout
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
mb="${MINIBOX_DIR:-$root/../chimera-main/extern/chimera-common-minibox}"
out="${Q88_OUT:-$root/build}"
mkdir -p "$out"
python3 "$root/tools/gen-config.py" >/dev/null
lib="${MINIBOX_HOST:-}"
if [ -z "$lib" ]; then
	for c in "$mb/build/source/host/libminiboxhost.a" "$mb/build/meson-linux/source/host/libminiboxhost.a"; do
		[ -f "$c" ] && { lib="$c"; break; }
	done
fi
[ -n "$lib" ] && [ -f "$lib" ] || { echo "no libminiboxhost.a (set MINIBOX_HOST)" >&2; exit 1; }
gcc -O2 -g -Wall -I"$mb/source/host" -I"$here" -o "$out/run-wbx" "$here/run-wbx.c" "$lib" -lpthread -lm
echo "OK: $out/run-wbx"
