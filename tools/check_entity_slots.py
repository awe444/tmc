#!/usr/bin/env python3
"""Check that no entity subtype struct overflows a gEntities pool slot.

Every entity in the game lives in a fixed-size slot: `gEntities` is
`GenericEntity[72]`, and each enemy/object/NPC subtype is a struct that opens
with `Entity base` and overlays that slot.  On the GBA every one of them fits
by construction.  Here they do not automatically: an `Entity*` is 8 bytes
rather than 4, so a subtype that spells its extra area with pointers can grow
past `sizeof(GenericEntity)` — and a write to a field past the end lands on the
*next entity in the pool*, whose first member is `prev`.

That is B63: `MoldormEntity` was 192 bytes against a 184-byte slot, so
`sub_08022EAC` wrote the segment's packed animation states over the low half of
the next entity's `prev` pointer, and the next `DeleteEntity` on that entity
dereferenced it.

The check reads DWARF out of a built binary rather than parsing C, because the
thing being checked is what the compiler actually laid out — the same reason
B61 argues for asserting offsets instead of commenting them.  Structs carrying
a `PORT_STATIC_ASSERT_EXPR(sizeof(X) <= sizeof(GenericEntity), ...)` are
already guarded at compile time; this catches the ones that are not.

Usage:
    tools/check_entity_slots.py [binary]        # default build/pc/tmc_pc

Exits non-zero if any subtype overflows.  Note this only sees structs that the
binary's debug info contains, so build with `-g` (the release config does).
"""

import re
import shutil
import subprocess
import sys

TAG = re.compile(r"^(0x[0-9a-f]+):(\s+)DW_TAG_(\w+)\s*$")
ATTR = re.compile(r"^\s+DW_AT_(\w+)\t\((.*)\)\s*$")
NULL = re.compile(r"^0x[0-9a-f]+:(\s+)NULL\s*$")
REF = re.compile(r"^\(?(0x[0-9a-f]+)")

SLOT_TYPE = "GenericEntity"


def dwarfdump(binary):
    exe = None
    for cand in ("llvm-dwarfdump", "llvm-dwarfdump-18", "llvm-dwarfdump-17", "llvm-dwarfdump-16"):
        if shutil.which(cand):
            exe = cand
            break
    if exe is None:
        sys.exit("need llvm-dwarfdump on PATH (apt install llvm)")
    proc = subprocess.Popen(
        [exe, "--debug-info", binary],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        errors="replace",
        bufsize=1 << 20,
    )
    return proc


def collect(binary):
    """Return (slot_size, {(file, line, name): size}) for entity-overlay structs.

    `GenericEntity` and most subtypes are anonymous structs behind a typedef,
    so struct DIEs are keyed by offset and the typedef names are resolved onto
    them afterwards.
    """
    proc = dwarfdump(binary)
    stack = []
    by_offset = {}   # struct DIE offset -> (size, decl_file, decl_line, own_name, is_overlay)
    typedefs = {}    # target DIE offset -> typedef name

    def flush(entry):
        offset, _, tag, attrs, members = entry
        if tag == "typedef":
            target = REF.match(attrs.get("type", ""))
            if target and "name" in attrs:
                typedefs.setdefault(target.group(1), attrs["name"].strip('"'))
            return
        if tag != "structure_type":
            return
        raw = attrs.get("byte_size")
        if raw is None:
            return
        size = int(raw, 16) if raw.startswith("0x") else int(raw)
        overlay = False
        if members:
            first = members[0]
            ftype = first.get("type", "")
            # A subtype overlays the slot if it opens with the common header.
            overlay = first.get("name") == '"base"' and ('"Entity"' in ftype or '"Enemy"' in ftype)
        by_offset[offset] = (
            size,
            attrs.get("decl_file", '"?"').strip('"'),
            attrs.get("decl_line", "?"),
            attrs.get("name", "").strip('"'),
            overlay,
        )

    for line in proc.stdout:
        m = TAG.match(line)
        if m:
            indent = len(m.group(2))
            while stack and stack[-1][1] >= indent:
                flush(stack.pop())
            stack.append([m.group(1), indent, m.group(3), {}, []])
            continue
        m = NULL.match(line)
        if m:
            indent = len(m.group(1))
            while stack and stack[-1][1] >= indent:
                flush(stack.pop())
            continue
        m = ATTR.match(line)
        if m and stack:
            top = stack[-1]
            top[3][m.group(1)] = m.group(2)
            if top[2] == "member" and len(stack) >= 2:
                parent = stack[-2]
                if parent[2] == "structure_type" and top[3] not in parent[4]:
                    parent[4].append(top[3])
    while stack:
        flush(stack.pop())
    proc.stdout.close()
    proc.wait()

    slot_size = None
    structs = {}
    for offset, (size, path, line, own, overlay) in by_offset.items():
        name = own or typedefs.get(offset, "<anon>")
        if name == SLOT_TYPE:
            slot_size = size
        if overlay:
            structs[(path, line, name)] = size
    return slot_size, structs


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "build/pc/tmc_pc"
    slot, structs = collect(binary)
    if slot is None:
        sys.exit(f"{binary}: no {SLOT_TYPE} in debug info — is it built with -g?")
    if not structs:
        sys.exit(f"{binary}: no entity-overlay structs found in debug info")

    over = {k: v for k, v in structs.items() if v > slot}
    print(f"{binary}: {len(structs)} entity-overlay structs, slot = {slot} bytes")
    if not over:
        print("OK — every subtype fits its pool slot")
        return 0
    print(f"\nFAIL — {len(over)} subtype(s) overflow the slot:")
    for (path, line, name), size in sorted(over.items(), key=lambda kv: -kv[1]):
        print(f"  {size:4d} bytes (+{size - slot})  {name}  {path}:{line}")
    print(
        "\nA write to a field past the slot lands on the next entity's `prev`.\n"
        "Overlay the roles that never share an entity (moldorm.c), or move the\n"
        "field into the trailing padding (mazaalBracelet.c)."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
