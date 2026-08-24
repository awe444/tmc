#!/usr/bin/env python3
"""Convert a tmc.sav between the port's layout and mGBA/hardware's.

Two things differ, and both have to be undone:

1. **Block byte order (B47).** The port stores each 8-byte EEPROM block in the
   game's native (little-endian u64) order; real hardware and every emulator
   store the order the bits go over the serial line, which is the reverse.

2. **`SaveFile.flags` is one byte out (B51).** `KinstoneSave`'s members sum to
   327 bytes, but the GBA layout `include/save.h` documents for the fields
   around it — `kinstones` at 0x114, `flags` at 0x25C — leaves 328. It is all
   `u8` arrays, so no padding makes up the difference. The port therefore
   writes `flags[0x200]` and the three `dungeon*` arrays one byte earlier than
   the real game reads them, and `darknut_timer` onward realigns because the
   compiler inserts a 1-byte hole for u32 alignment — which is why the struct
   is coincidentally the right total size and nothing caught it.

   Everything before `flags` (name, stats, inventory, the kinstone bag) is at
   the correct offset, so a converted save looks *almost* right on hardware:
   correct hearts and elements, but every story flag shifted a bit, which
   presents as Link missing Ezlo and world-changing events being un-done.

Shifting the region moves bytes, so each affected save slot's checksum is
recomputed with the game's own algorithm (`CalculateChecksum`, src/save.c).
Only slots whose stored checksum already verified are touched: an empty or
deleted slot keeps its deliberately-invalid status rather than being blessed
into looking real.

The direction is taken from the signature in block 0, so running the tool
twice returns the original file — with one exception. Converting *into* the
port's layout discards the GBA byte at `SaveFile+603`, because the port's
short `KinstoneSave` has no field for it; it is reported on stderr and comes
back as 0. That asymmetry is the port bug showing through, not a tool defect,
and it disappears when the struct is fixed.

    savconv.py <in.sav> <out.sav>

**When `KinstoneSave` is fixed in the port, delete `relayout()` and its call
sites here** — the port would then already write the hardware layout and this
would silently corrupt every save it touched. `LAYOUT_FIXED_IN_PORT` below is
the switch to flip.
"""
import struct
import sys

LAYOUT_FIXED_IN_PORT = False  # set True once KinstoneSave is 328 bytes

# src/save.c: gSaveFileEEPROMAddresses — (size, status_a, status_b, data_a, data_b)
SAVE_SLOTS = [
    (0x500, 0x30, 0x1030, 0x080, 0x1080),  # save 0
    (0x500, 0x40, 0x1040, 0x580, 0x1580),  # save 1
    (0x500, 0x50, 0x1050, 0xA80, 0x1A80),  # save 2
]
STATUS_MAGIC = 0x4D435A33  # 'MCZ3', reads as "3ZCM" in a hex dump

# SaveFile offsets. The port puts flags here; the GBA puts it one byte later.
FLAGS_PORT = 603
FLAGS_GBA = 604
# darknut_timer, which both layouts agree on (u32 alignment absorbs the byte).
REALIGN_END = 1164


def calc_checksum(buf, off, size):
    """CalculateChecksum from src/save.c: sum of (u16 ^ remaining_size)."""
    total = 0
    while size:
        total = (total + (struct.unpack_from('<H', buf, off)[0] ^ size)) & 0xFFFFFFFF
        off += 2
        size -= 2
    return total & 0xFFFF


def slot_checksum(logical, data_off, size):
    """VerifyChecksum sums the 4-byte status word and then the record."""
    return (calc_checksum(STATUS_MAGIC.to_bytes(4, 'little'), 0, 4)
            + calc_checksum(logical, data_off, size)) & 0xFFFF


def slot_is_valid(logical, size, status_off, data_off):
    stored, negated, magic = struct.unpack_from('<HHI', logical, status_off)
    return (magic == STATUS_MAGIC
            and negated == ((-stored) & 0xFFFF)
            and stored == slot_checksum(logical, data_off, size))


def relayout(logical, to_gba):
    """Move flags..dungeonWarps between the port's layout and the GBA's."""
    moved = 0
    for size, status_a, status_b, data_a, data_b in SAVE_SLOTS:
        if not slot_is_valid(logical, size, status_a, data_a):
            continue  # empty, deleted, or already damaged — leave it alone
        for data_off in (data_a, data_b):
            rec = bytearray(logical[data_off:data_off + size])
            if to_gba:
                block = rec[FLAGS_PORT:REALIGN_END - 1]
                rec[FLAGS_PORT] = 0  # the byte KinstoneSave is missing
                rec[FLAGS_GBA:REALIGN_END] = block
            else:
                # Going this way, the GBA's byte at FLAGS_PORT is the one
                # KinstoneSave is missing, and the port's struct has nowhere
                # to put it: it is discarded. Say so rather than lose it
                # quietly — it comes back as 0 if converted out again.
                dropped = rec[FLAGS_PORT]
                if dropped:
                    print(f"  note: discarding KinstoneSave's last byte "
                          f"(0x{dropped:02X}) at SaveFile+{FLAGS_PORT} of the record "
                          f"at 0x{data_off:04X} — the port's struct has no field "
                          f"for it", file=sys.stderr)
                block = rec[FLAGS_GBA:REALIGN_END]
                rec[FLAGS_PORT:REALIGN_END - 1] = block
                rec[REALIGN_END - 1] = 0  # the port's alignment hole
            logical[data_off:data_off + size] = rec
        new = slot_checksum(logical, data_a, size)
        for status_off in (status_a, status_b):
            struct.pack_into('<HHI', logical, status_off,
                             new, (-new) & 0xFFFF, STATUS_MAGIC)
        moved += 1
    return moved


def swap_blocks(data):
    return bytearray(b''.join(data[i:i + 8][::-1] for i in range(0, len(data), 8)))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip().splitlines()[-1])
    src, dst = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, 'rb').read())

    # The signature is plain text in the port's order and reversed in mGBA's.
    if data[:8] == b'AGBZELDA':
        to_gba = True
    elif data[:8] == b'ADLEZBGA':
        to_gba = False
    else:
        sys.exit(f"{src}: not a TMC save — block 0 is {bytes(data[:8])!r}, "
                 "expected b'AGBZELDA' (port) or b'ADLEZBGA' (mGBA/hardware)")

    # Work in the game's logical byte order throughout: that is what the
    # checksum is computed over, and what the field offsets refer to.
    logical = data if to_gba else swap_blocks(data)
    moved = 0 if LAYOUT_FIXED_IN_PORT else relayout(logical, to_gba)
    out = swap_blocks(logical) if to_gba else logical

    open(dst, 'wb').write(bytes(out))
    where = "port -> mGBA/hardware" if to_gba else "mGBA/hardware -> port"
    note = f", {moved} slot(s) realigned" if moved else ", no valid slots to realign"
    print(f"{src} -> {dst}  ({where}, {len(out)} bytes, "
          f"block0 {bytes(out[:8])!r}{note})")


if __name__ == '__main__':
    main()
