#!/usr/bin/env python3
"""Convert a tmc.sav between the port's layout and mGBA/hardware's.

The port stores each 8-byte EEPROM block in the game's native (little-endian
u64) order; real hardware and every emulator store the order the bits go over
the serial line, which is the reverse. The conversion is its own inverse.
"""
import sys
d = open(sys.argv[1], 'rb').read()
out = bytearray()
for i in range(0, len(d), 8):
    out += d[i:i+8][::-1]
open(sys.argv[2], 'wb').write(bytes(out))
print(f"{sys.argv[1]} -> {sys.argv[2]}  ({len(out)} bytes, block0 {bytes(out[:8])!r})")
