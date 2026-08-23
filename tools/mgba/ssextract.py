#!/usr/bin/env python3
"""Split an mGBA savestate into its machine state and its screenshot.

mGBA writes a savestate as a PNG whose *image* is the frame it was taken on and
whose `gbAs` chunk is the deflated GBASerializedState. That pairing is what
makes a savestate an oracle: the state and the pixels it produced come from one
file, so "why is this pixel this colour" can be answered outright instead of
inferred. See README.md.

Usage:  ssextract.py <file.ss1> <state.bin> <shot.raw>

`shot.raw` is 240x160 RGB888, row-major, no header.
"""
import struct, zlib, sys

src, outstate, outraw = sys.argv[1], sys.argv[2], sys.argv[3]
d = open(src, 'rb').read()
if d[:8] != b'\x89PNG\r\n\x1a\n':
    sys.exit(f"{src}: not a PNG — mGBA savestates are PNG-wrapped")

i = 8; idat = b''; blob = None
while i < len(d):
    ln = struct.unpack_from('>I', d, i)[0]
    typ = d[i+4:i+8]
    if typ == b'IDAT':
        idat += d[i+8:i+8+ln]
    elif typ == b'gbAs':
        blob = d[i+8:i+8+ln]
    i += 12 + ln
if blob is None:
    sys.exit(f"{src}: no gbAs chunk — not an mGBA savestate")
open(outstate, 'wb').write(zlib.decompress(blob))

raw = zlib.decompress(idat)
W, H, bpp = 240, 160, 3
stride = W * bpp
img = bytearray(W * H * bpp)
prev = bytearray(stride)
p = 0
for y in range(H):                      # undo the per-row PNG filters
    f = raw[p]; p += 1
    line = bytearray(raw[p:p+stride]); p += stride
    for x in range(stride):
        a = line[x-bpp] if x >= bpp else 0
        b = prev[x]
        c = prev[x-bpp] if x >= bpp else 0
        if   f == 1: line[x] = (line[x] + a) & 0xFF
        elif f == 2: line[x] = (line[x] + b) & 0xFF
        elif f == 3: line[x] = (line[x] + ((a + b) >> 1)) & 0xFF
        elif f == 4:
            pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2*c)
            pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
            line[x] = (line[x] + pr) & 0xFF
    img[y*stride:(y+1)*stride] = line
    prev = line
open(outraw, 'wb').write(bytes(img))
print(f"{src}: state -> {outstate} ({len(zlib.decompress(blob))} bytes), "
      f"screenshot -> {outraw} (240x160 RGB)")
