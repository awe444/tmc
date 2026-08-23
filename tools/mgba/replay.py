#!/usr/bin/env python3
"""Replay a capture script into mGBA's CLI debugger.

TMC reads KEYINPUT once per frame at 0x0801D6C4 (`ldrh r0,[r0]`), so a read
watchpoint on 0x04000130 is a per-frame breakpoint with the raw value already
in r0; `w/r r0` overwrites it before `bic r1, r0` turns it into the pressed
mask. Our capture scripts already store GBA KEYINPUT bit masks.

mGBA counts from reset and the port skips the BIOS, so the two frame origins
differ; --offset shifts the script's frames onto mGBA's clock.
"""
import sys, argparse

KEYS = {"A":1,"B":2,"SELECT":4,"START":8,"RIGHT":0x10,"LEFT":0x20,
        "UP":0x40,"DOWN":0x80,"R":0x100,"L":0x200,"NONE":0}

def load(path):
    ev = {}
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        p = line.split()
        if len(p) >= 3 and p[1] == 'keys':
            m = 0
            for tok in p[2].split('+'):
                m |= KEYS[tok]
            ev[int(p[0])] = m
    return ev

ap = argparse.ArgumentParser()
ap.add_argument('script'); ap.add_argument('last', type=int)
ap.add_argument('--offset', type=int, default=0)
ap.add_argument('--probe-every', type=int, default=0)
ap.add_argument('--dump', default='')
a = ap.parse_args()

ev = {f + a.offset: m for f, m in load(a.script).items()}
out = ["watch/r 0x04000130"]
dumps = {int(x) for x in a.dump.split(',') if x}
cur = 0
for f in range(a.last + 1):
    out.append("c")
    if f in ev:
        cur = ev[f]
    out.append("w/r r0 0x%X" % (0x3FF ^ cur))
    if a.probe_every and f % a.probe_every == 0:
        out.append("print/x 0x%X" % f)
        out.append("x/2 0x04000000 8")
    if f in dumps:
        out.append("print/x 0x%X" % f)
        out.append("x/2 0x04000000 8")
        out.append("x/2 0x07000000 512")
        out.append("x/2 0x060110A0 16")
out.append("quit")
print("\n".join(out))
