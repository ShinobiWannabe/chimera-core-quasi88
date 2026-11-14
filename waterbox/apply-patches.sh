#!/bin/sh
# apply-patches.sh - applies patches/*.patch to extern/QUASI88, in order.
#
# All or nothing (docs/porting-a-core.md): a clean upstream tree gets the
# whole series; a tree the series already applies to in reverse is taken to
# have it; anything else stops with a message. Every patch is a hook or a
# build option, never a deletion.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
q=$root/extern/QUASI88
[ -f "$q/src/quasi88.c" ] || { echo "no QUASI88 at $q (git submodule update --init)" >&2; exit 1; }
# an empty folder answers git for the repository above it
top=$(git -C "$q" rev-parse --show-toplevel 2>/dev/null || true)
[ "$(cd "$top" 2>/dev/null && pwd)" = "$(cd "$q" && pwd)" ] || { echo "$q is not its own git checkout" >&2; exit 1; }
set -- "$root"/patches/*.patch
if git -C "$q" apply --check "$@" 2>/dev/null; then
	git -C "$q" apply "$@"
	echo "patches applied: $#"
elif git -C "$q" apply --reverse --check "$@" 2>/dev/null; then
	echo "patches already applied: $#"
else
	echo "extern/QUASI88 is neither clean nor patched; reset it (git -C extern/QUASI88 checkout .) and run again" >&2
	exit 1
fi
