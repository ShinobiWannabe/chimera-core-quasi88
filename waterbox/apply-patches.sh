#!/bin/sh
# apply-patches.sh - applies patches/*.patch to extern/QUASI88, in order.
#
# All or nothing (docs/porting-a-core.md): a clean upstream tree gets the
# whole series; a tree that already is upstream plus the whole series is
# left as it is; anything else stops with a message. Every patch is a hook
# or a build option, never a deletion.
#
# The patches go on one at a time, since two of them may change the same
# file (git apply given several patches at once cannot do that). To tell
# whether the tree already has them, the series is applied to a scratch
# index and the result compared with the tree.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
q=$root/extern/QUASI88
[ -f "$q/src/quasi88.c" ] || { echo "no QUASI88 at $q (git submodule update --init)" >&2; exit 1; }
# an empty folder answers git for the repository above it
top=$(git -C "$q" rev-parse --show-toplevel 2>/dev/null || true)
[ "$(cd "$top" 2>/dev/null && pwd)" = "$(cd "$q" && pwd)" ] || { echo "$q is not its own git checkout" >&2; exit 1; }
set -- "$root"/patches/*.patch

# upstream plus the whole series, as a tree, built in a scratch index
scratch=$(mktemp "${TMPDIR:-/tmp}/q88-index.XXXXXX")
trap 'rm -f "$scratch"' EXIT
GIT_INDEX_FILE=$scratch git -C "$q" read-tree HEAD
for p in "$@"; do
	GIT_INDEX_FILE=$scratch git -C "$q" apply --cached "$p" || { echo "$p does not apply to upstream" >&2; exit 1; }
done
patched=$(GIT_INDEX_FILE=$scratch git -C "$q" write-tree)

if git -C "$q" diff --quiet "$patched" --; then
	echo "patches already applied: $#"
elif git -C "$q" diff --quiet HEAD --; then
	for p in "$@"; do git -C "$q" apply "$p"; done
	echo "patches applied: $#"
else
	echo "extern/QUASI88 is neither clean nor patched; reset it (git -C extern/QUASI88 checkout .) and run again" >&2
	exit 1
fi
