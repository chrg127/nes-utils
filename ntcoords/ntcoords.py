#!/usr/bin/env python3
import sys

def error(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

def isnum(n):
    try: return int(n)
    except:
        error("error: not a number: " + str(n))
        sys.exit(1)

def ntaddr(x, y):
    return 0x2000 + int(y/8)*32 + int(x/8)

if len(sys.argv) < 3:
    error("usage: ntcoord x y")
    sys.exit(1)

x = isnum(sys.argv[1])
y = isnum(sys.argv[2])

if x >= 256 or y >= 240:
    print("out of bounds")
else:
    print("X".format(ntaddr(x, y)))

