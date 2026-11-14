#!/usr/bin/env python3
"""type-keys.py - a Q88_PRESS script (waterbox/press-script.h) that types text.

Usage: type-keys.py <first frame> <text>
In the text, | is RETURN and ~ is a double quote (SHIFT and 2). Letters,
digits, space, colon and period are their keys. Each key is held 3 frames,
6 frames apart; after RETURN there are 20 frames more, for BASIC to answer.
The frame after the last key goes to stderr as "end <frame>".
"""
import sys

start, text = int(sys.argv[1]), sys.argv[2]
keys = {" ": "Key SPACE", "|": "Key RETURN", ":": "Key Colon", ".": "Key Period"}
f, out = start, []
for ch in text:
    if ch == "~":
        out += ["%d:Key SHIFT:12" % f, "%d:Key 2:4" % (f + 4)]
        f += 16
        continue
    out.append("%d:%s:3" % (f, keys.get(ch) or "Key " + ch.upper()))
    f += 28 if ch == "|" else 6
print(",".join(out))
sys.stderr.write("end %d\n" % f)
