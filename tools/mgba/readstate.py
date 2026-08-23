#!/usr/bin/env python3
"""Print the display state out of an mGBA savestate blob.

Usage:  ssextract.py <file.ss1> state.bin shot.raw
        readstate.py state.bin [x y]

With x/y it also reports, for that pixel, every BG layer's palette index and
priority and every OBJ covering it — which is what settles "why is this pixel
this colour" against hardware. See README.md.
"""
import struct, sys

PRAM, OAM, VRAM, IO = 0x800, 0xC00, 0x1000, 0x400
SZ = {(0,0):(8,8),(0,1):(16,16),(0,2):(32,32),(0,3):(64,64),
      (1,0):(16,8),(1,1):(32,8),(1,2):(32,16),(1,3):(64,32),
      (2,0):(8,16),(2,1):(8,32),(2,2):(16,32),(2,3):(32,64)}

st = open(sys.argv[1], 'rb').read()
def r(off): return struct.unpack_from('<H', st, IO + off)[0]

print(f"DISPCNT={r(0):04X}  BLDCNT={r(0x50):04X}  WININ={r(0x48):04X} WINOUT={r(0x4A):04X}")
for k in range(4):
    c = r(8 + 2*k)
    print(f"  BG{k}CNT={c:04X}  prio={c & 3} charbase={(c >> 2) & 3} screenbase={(c >> 8) & 0x1F} "
          f"256col={(c >> 7) & 1} size={c >> 14} scroll=({r(0x10 + 4*k)},{r(0x12 + 4*k)}) "
          f"{'on' if (r(0) >> (8 + k)) & 1 else 'OFF'}")

if len(sys.argv) < 4:
    sys.exit(0)
X, Y = int(sys.argv[2]), int(sys.argv[3])
print(f"\npixel ({X},{Y}):")
for k in range(4):
    c = r(8 + 2*k)
    if not (r(0) >> (8 + k)) & 1:
        continue
    bx, by = X + r(0x10 + 4*k), Y + r(0x12 + 4*k)
    e = struct.unpack_from('<H', st, VRAM + ((c >> 8) & 0x1F)*0x800
                           + 2*((((by >> 3) & 31)*32) + ((bx >> 3) & 31)))[0]
    t = e & 0x3FF; px, py = bx & 7, by & 7
    if e & 0x400: px = 7 - px
    if e & 0x800: py = 7 - py
    b = st[VRAM + ((c >> 2) & 3)*0x4000 + t*32 + py*4 + (px >> 1)]
    idx = (b & 0xF) if (px & 1) == 0 else (b >> 4)
    print(f"  BG{k} prio={c & 3} tile={t} idx={idx} {'opaque' if idx else 'transparent'}")
print("  OBJs covering it, in OAM order (the LAST one sets the layer's priority):")
for e in range(128):
    a0, a1, a2 = struct.unpack_from('<3H', st, OAM + e*8)
    if (a0 & 0x300) == 0x200 or (a0 == 0 and a1 == 0 and a2 == 0):
        continue
    w, h = SZ[(a0 >> 14, a1 >> 14)]; oy = a0 & 0xFF; ox = a1 & 0x1FF
    if ox >= 304: ox -= 512
    if not (oy <= Y < oy + h and ox <= X < ox + w):
        continue
    px, py = X - ox, Y - oy
    if a1 & 0x1000: px = w - 1 - px
    if a1 & 0x2000: py = h - 1 - py
    tn = (a2 & 0x3FF) + (py >> 3)*(w >> 3) + (px >> 3)
    b = st[VRAM + 0x10000 + tn*32 + (py & 7)*4 + ((px & 7) >> 1)]
    idx = (b & 0xF) if ((px & 7) & 1) == 0 else (b >> 4)
    print(f"    OAM[{e:3d}] prio={(a2 >> 10) & 3} objmode={(a0 >> 10) & 3} tile={a2 & 0x3FF:4d} "
          f"pal={a2 >> 12:2d} {w}x{h:<2} idx={idx} {'OPAQUE' if idx else 'transparent'}")
