#!/usr/bin/env python3
"""
Host-side asset-table generator for the SDL port.

The matching GBA ROM build assembles `data/gfx/gfx_and_palettes.s` with
`.incbin` directives that pull binary blobs out of `assets/`, after the
existing C++ `tools/asset_processor` has extracted those blobs from a
copy of `baserom.gba`. The SDL host build is decoupled from the ROM
build (see `docs/sdl_port.md`'s "Coexistence with the GBA ROM build"
section) and cannot use GAS, so this Python tool produces an equivalent
host-side C source from the same inputs.

Inputs
------
* `--baserom PATH`     - the player's own `baserom.gba` (16,777,216 bytes
                         for the retail ROMs). Or the deterministic
                         placeholder produced by `make-placeholder-baserom`.
* `--variant {USA,EU,JP,DEMO_USA,DEMO_JP}`
                       - the GAME_VERSION the build was configured with.
* `--out-dir DIR`      - where to write the generated headers / sources.
* `--repo-root DIR`    - repository root (defaults to two levels up from
                         this script). Used to find `assets/gfx.json`,
                         `data/gfx/gfx_groups.s`, `data/gfx/palette_groups.s`.

Outputs (under --out-dir)
-------------------------
* `gfx_offsets.h`      - real `#define offset_X 0x...` values for every
                         palette / gfx asset that this variant ships,
                         then `#include "_port_offset_stubs.h"` for the
                         catch-all zeros.
* `port_rom_assets.c`  - strong host definitions of:
                            - `gGlobalGfxAndPalettes[]` (the concatenated
                              palettes + gfx blob, sized to fit every asset
                              that any variant references)
                            - `gGfxGroups[]` (parsed from
                              `data/gfx/gfx_groups.s`, with `.ifdef`/`.else`
                              chains resolved for the active variant)
                            - `gPaletteGroups[]` (parsed from
                              `data/gfx/palette_groups.s`, similarly)
                            - `gFrameObjLists[]` (extracted from baserom
                              per `assets/assets.json`; consumed by
                              `affine.c::DrawDirect` -> `port_oam_renderer.c`
                              for direct sprite emission)

When the host CMake build is configured with `-DTMC_BASEROM=...`, this
tool runs at configure time and the generated source replaces
`src/platform/shared/port_rom_data_stubs.c`. With no baserom configured
the existing all-zero stubs are linked instead, which is the documented
default (see `docs/sdl_port.md`'s "Game assets / `baserom.gba`" section).

Subcommands
-----------
    gen                       Run the full pipeline.
    make-placeholder-baserom  Write a deterministic 16 MiB all-zero file
                              suitable for exercising the pipeline in CI
                              and for early-bringup local testing without
                              shipping any copyrighted data.
    gen-offsets-stub          Regenerate the all-zero
                              `_port_offset_stubs.h` catch-all that lives
                              under `src/platform/shared/generated/assets/`.
                              No baserom required. Replaces the long-
                              referenced-but-never-landed
                              `gen_offset_stubs.py`.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# ---------------------------------------------------------------------------
# Variant table.
#
# `JP_D` is a derived flag set whenever `JP` or `DEMO_JP` is the build
# variant (see the preamble of `data/gfx/gfx_groups.s`). The `data/gfx/*.s`
# files only branch on `EU`, `JP`, `DEMO_JP`, and `JP_D`; we still expose
# every variant token so future asset tables can branch on `DEMO_USA` /
# `USA` / `ENGLISH` if needed.
# ---------------------------------------------------------------------------
ALL_VARIANTS = ("USA", "EU", "JP", "DEMO_USA", "DEMO_JP")


def variant_defines(variant: str) -> Set[str]:
    if variant not in ALL_VARIANTS:
        raise SystemExit(f"unknown variant {variant!r}; one of {ALL_VARIANTS}")
    defines = {variant}
    if variant in ("JP", "DEMO_JP"):
        defines.add("JP_D")
    return defines


# ---------------------------------------------------------------------------
# gfx.json walking. The schema (see `assets/gfx.json` and the matching C++
# walker in `tools/src/asset_processor/main.cpp`) is:
#
#   { "offsets": { "EU": -2736, "JP": -864, "DEMO_USA": 2608, "DEMO_JP": -872 } }
#       Sets the variant offset that gets added to every following `start`.
#
#   { "calculateOffsets": "gfx_offsets.inc", "start": 5910144 }
#       Marks the start of an offset-calculation block; subsequent assets
#       have their (variant-adjusted) `start` rebased against this value to
#       produce the `offset_<symbol>` macros.
#
#   { "path": "palettes/gPalette_0.gbapal", "start": 5910144, "size": 32,
#     "type": "palette" [, "variants": ["USA", "JP", ...]] }
#       A single asset. The symbol is derived from the path stem-twice
#       (e.g. `path/foo.4bpp.lz` -> `foo`) -- see BaseAsset::getSymbol() in
#       `tools/src/asset_processor/assets/asset.h`.
# ---------------------------------------------------------------------------


@dataclass
class GfxAsset:
    """One entry from gfx.json, rebased for the active variant."""

    symbol: str          # e.g. "gPalette_0", "gGfx_1_0"
    file_offset: int     # byte offset into the baserom (variant-adjusted)
    size: int            # bytes to read
    block_offset: int    # byte offset into gGlobalGfxAndPalettes


def stem_twice(path: str) -> str:
    """Match BaseAsset::getSymbol() in tools/src/asset_processor: strip the
    extension twice so e.g. ``foo.4bpp.lz`` -> ``foo``.
    """
    stem = os.path.basename(path)
    stem = os.path.splitext(stem)[0]
    stem = os.path.splitext(stem)[0]
    return stem


def load_gfx_assets(repo_root: Path, variant: str) -> Tuple[List[GfxAsset], int]:
    """Walk assets/gfx.json for the active variant.

    Returns (assets, base_file_offset). `base_file_offset` is the
    variant-adjusted start of the `gGlobalGfxAndPalettes` block (the value
    that gets subtracted from each asset's file offset to produce the
    `offset_<symbol>` macros).
    """
    cfg_path = repo_root / "assets" / "gfx.json"
    with cfg_path.open() as f:
        entries = json.load(f)

    variant_off = 0
    base_file_offset: Optional[int] = None
    assets: List[GfxAsset] = []

    for entry in entries:
        if "offsets" in entry:
            variant_off = entry["offsets"].get(variant, 0)
            continue
        if "calculateOffsets" in entry:
            # We only care about the gfx_offsets.inc block; gfx.json only
            # has the one such marker, but be explicit anyway.
            if entry["calculateOffsets"] != "gfx_offsets.inc":
                continue
            base_file_offset = entry["start"] + variant_off
            continue
        if "path" not in entry:
            continue
        if "variants" in entry and variant not in entry["variants"]:
            continue
        if base_file_offset is None:
            # Defensive: gfx.json declares calculateOffsets before any
            # asset entry, so this should never trigger.
            continue
        if "start" in entry:
            file_offset = entry["start"] + variant_off
        elif "starts" in entry:
            file_offset = entry["starts"][variant]
        else:
            continue
        size = entry.get("size", 0)
        symbol = stem_twice(entry["path"])
        assets.append(
            GfxAsset(
                symbol=symbol,
                file_offset=file_offset,
                size=size,
                block_offset=file_offset - base_file_offset,
            )
        )

    if base_file_offset is None:
        raise SystemExit(
            f"{cfg_path}: no `calculateOffsets: gfx_offsets.inc` marker"
        )
    return assets, base_file_offset


# ---------------------------------------------------------------------------
# `data/gfx/*.s` parsing.
#
# We honour exactly the directives the two files actually use: `.ifdef X`
# / `.else` / `.endif` / `.ifndef X`, plus the four custom macros
# `gfx_raw`, `palette_set`, `enum_start`, `enum`. Everything else (`.4byte`
# `.2byte` `.byte`, `.include`, `.section`, `.align`, label declarations
# `gGfxGroup_N::` / `gPaletteGroup_N::` / `JP_D::`) we recognise where
# necessary and ignore otherwise.
#
# The macros mirror the GAS definitions in `asm/macros/gfx.inc`:
#   gfx_raw src=expr, unknown=0, dest=0, size=0, compressed=0, terminator=0
#       => .4byte (!terminator << 0x1F) + src + unknown * 0x1000000,
#                 dest,
#                 size + (compressed << 0x1F)
#   palette_set palette=expr, offset=0, count=0, terminator=0
#       => .2byte palette
#          .byte offset, count & 0xf + (!terminator * 0x80)
#   enum_start [N=0]      => set __enum__ = N
#   enum NAME             => .equiv NAME, __enum__; __enum__ += 1
# ---------------------------------------------------------------------------


@dataclass
class GfxRaw:
    """One row inside a `gGfxGroup_N` table.

    Fields match the host `GfxItem` struct (`port_rom_data_types.h`):
        unk0  =  ((!terminator) << 31) | (unknown << 24) | (offset & 0xFFFFFF)
        dest  =  raw GBA address
        unk8  =  size | (compressed << 31)
    """

    src_symbol: str   # `offset_*` reference; resolved later
    unknown: int      # 4-bit ctrl field (0x7 / 0xD / ...)
    dest: int         # GBA hardware address (0x06000000, 0x02034570, ...)
    size: int         # bytes to copy
    compressed: bool
    terminator: bool


@dataclass
class PaletteSet:
    """One row inside a `gPaletteGroup_N` table.

    Fields match the host `PaletteGroup` struct (`port_rom_data_types.h`):
        paletteId      = expr (the enum value, * 32 == byte offset)
        destPaletteNum = offset
        numPalettes    = (count & 0xF) | (!terminator << 7)
    """

    palette_expr: str  # `pal_NNN` reference; resolved against the enum table
    offset: int
    count: int
    terminator: bool


# Lightweight tokeniser. `key=value` arguments may be separated by ``,``
# or by whitespace; values are limited to bare identifiers and integer
# literals (decimal or 0x-prefixed). This is exactly the subset GAS
# itself accepts inside the four macros above.
_ARG_RE = re.compile(r"([a-zA-Z_][a-zA-Z_0-9]*)\s*=\s*([a-zA-Z_0-9]+)")


def parse_int(token: str) -> int:
    if token.startswith("0x") or token.startswith("0X"):
        return int(token, 16)
    if token.startswith("0b") or token.startswith("0B"):
        return int(token, 2)
    if token.lstrip("-").isdigit():
        return int(token)
    raise ValueError(f"not an integer literal: {token!r}")


def parse_gfx_args(rest: str) -> Dict[str, str]:
    """Pull `key=value` pairs out of a macro call's argument list."""
    return {m.group(1): m.group(2) for m in _ARG_RE.finditer(rest)}


@dataclass
class ParsedAsmFile:
    """Result of parsing one of `data/gfx/{gfx,palette}_groups.s`.

    `enum_table` only matters for palette_groups.s, but the same parser
    drives both files so it's recorded uniformly.
    """

    # gfx_raw: { "gGfxGroup_1": [GfxRaw, ...], ... }
    gfx_groups: Dict[str, List[GfxRaw]] = field(default_factory=dict)
    # palette_set: { "gPaletteGroup_1": [PaletteSet, ...], ... }
    palette_groups: Dict[str, List[PaletteSet]] = field(default_factory=dict)
    # The order in which `gGfxGroups::` / `gPaletteGroups::` enumerates
    # the per-group symbols. None placeholders model the `.4byte 0`
    # entries (the deliberately-NULL slot 0).
    gfx_index_order: List[Optional[str]] = field(default_factory=list)
    palette_index_order: List[Optional[str]] = field(default_factory=list)
    # `enum NAME` lookups, after `enum_start` resolution.
    enum_table: Dict[str, int] = field(default_factory=dict)


_LABEL_RE = re.compile(r"^([A-Za-z_][A-Za-z_0-9]*)::\s*$")
_GFXGROUP_RE = re.compile(r"^gGfxGroup_(\d+)$")
_PALGROUP_RE = re.compile(r"^gPaletteGroup_(\d+)$")
_FOUR_BYTE_RE = re.compile(r"^\s*\.4byte\s+(.+?)\s*$")


def parse_asm_file(path: Path, defines: Set[str]) -> ParsedAsmFile:
    """Parse a `data/gfx/*.s` file with `.ifdef`/`.else`/`.endif` and the
    custom `gfx_raw` / `palette_set` / `enum` macros, dropping branches
    whose define doesn't match the active variant.
    """
    out = ParsedAsmFile()
    enum_counter = 0
    # Stack of "is the surrounding block currently active?" booleans.
    # Top-of-stack is consulted on every line; `.if` pushes, `.else`
    # toggles, `.endif` pops. Nested blocks are an AND of all stack
    # entries -- but since `data/gfx/*.s` only nests at most two deep,
    # we just track the active flag explicitly.
    cond_stack: List[Tuple[bool, bool]] = []  # (cond_value, in_else_branch)

    def is_active() -> bool:
        for value, in_else in cond_stack:
            taken = (not value) if in_else else value
            if not taken:
                return False
        return True

    current_gfx: Optional[List[GfxRaw]] = None
    current_pal: Optional[List[PaletteSet]] = None
    in_gfx_index = False
    in_pal_index = False

    with path.open() as f:
        for raw_line in f:
            # Strip comments. GAS recognises `@` and `/* ... */`; we strip
            # the whole rest of the line on `@` (the only comment style
            # used in these two files).
            line = raw_line.split("@", 1)[0].rstrip()
            stripped = line.strip()
            if not stripped:
                continue

            # Conditionals: handled even when the current branch is
            # inactive (nested ifdefs need to balance correctly).
            if stripped.startswith(".ifdef "):
                sym = stripped.split(None, 1)[1].strip()
                cond_stack.append((sym in defines, False))
                continue
            if stripped.startswith(".ifndef "):
                sym = stripped.split(None, 1)[1].strip()
                cond_stack.append((sym not in defines, False))
                continue
            if stripped == ".else":
                if not cond_stack:
                    raise SystemExit(f"{path}: stray .else")
                value, _ = cond_stack[-1]
                cond_stack[-1] = (value, True)
                continue
            if stripped == ".endif":
                if not cond_stack:
                    raise SystemExit(f"{path}: stray .endif")
                cond_stack.pop()
                continue

            if not is_active():
                continue

            # Label declaration: `gGfxGroup_42::` / `gPaletteGroup_3::` /
            # `JP_D::` / `gGfxGroups::` / `gPaletteGroups::`.
            m = _LABEL_RE.match(stripped)
            if m:
                label = m.group(1)
                if label == "gGfxGroups":
                    in_gfx_index = True
                    in_pal_index = False
                    current_gfx = None
                    current_pal = None
                    continue
                if label == "gPaletteGroups":
                    in_gfx_index = False
                    in_pal_index = True
                    current_gfx = None
                    current_pal = None
                    continue
                in_gfx_index = False
                in_pal_index = False
                if _GFXGROUP_RE.match(label):
                    out.gfx_groups[label] = []
                    current_gfx = out.gfx_groups[label]
                    current_pal = None
                    continue
                if _PALGROUP_RE.match(label):
                    out.palette_groups[label] = []
                    current_pal = out.palette_groups[label]
                    current_gfx = None
                    continue
                # Other labels (e.g. `JP_D::`) we ignore; they're scope
                # markers, not data.
                continue

            # Inside an index table: `.4byte gGfxGroup_N` / `.4byte 0`.
            if in_gfx_index or in_pal_index:
                m = _FOUR_BYTE_RE.match(line)
                if not m:
                    continue
                expr = m.group(1).strip().rstrip(",").strip()
                target: Optional[str]
                if expr == "0":
                    target = None
                else:
                    target = expr
                if in_gfx_index:
                    out.gfx_index_order.append(target)
                else:
                    out.palette_index_order.append(target)
                continue

            # `enum_start [N]` and `enum NAME`.
            tokens = stripped.split()
            head = tokens[0]
            if head == "enum_start":
                if len(tokens) > 1:
                    enum_counter = parse_int(tokens[1])
                else:
                    enum_counter = 0
                continue
            if head == "enum":
                if len(tokens) < 2:
                    raise SystemExit(f"{path}: malformed enum: {stripped!r}")
                name = tokens[1]
                out.enum_table[name] = enum_counter
                enum_counter += 1
                continue

            # Macro calls.
            if head == "gfx_raw" and current_gfx is not None:
                args = parse_gfx_args(stripped[len(head):])
                if "src" not in args:
                    raise SystemExit(f"{path}: gfx_raw missing src: {stripped!r}")
                current_gfx.append(
                    GfxRaw(
                        src_symbol=args["src"],
                        unknown=parse_int(args.get("unknown", "0")),
                        dest=parse_int(args.get("dest", "0")),
                        size=parse_int(args.get("size", "0")),
                        compressed=parse_int(args.get("compressed", "0")) != 0,
                        terminator=parse_int(args.get("terminator", "0")) != 0,
                    )
                )
                continue
            if head == "palette_set" and current_pal is not None:
                args = parse_gfx_args(stripped[len(head):])
                if "palette" not in args:
                    raise SystemExit(
                        f"{path}: palette_set missing palette: {stripped!r}"
                    )
                current_pal.append(
                    PaletteSet(
                        palette_expr=args["palette"],
                        offset=parse_int(args.get("offset", "0")),
                        count=parse_int(args.get("count", "0")),
                        terminator=parse_int(args.get("terminator", "0")) != 0,
                    )
                )
                continue
            # Anything else (including `.section`, `.align`, `.include`,
            # etc.) is silently ignored; the host build does not care
            # about section placement or include paths.

    if cond_stack:
        raise SystemExit(f"{path}: unterminated conditional ({len(cond_stack)} open)")
    return out


# ---------------------------------------------------------------------------
# Code generation.
# ---------------------------------------------------------------------------


def write_offsets_header(out_path: Path, assets: List[GfxAsset]) -> None:
    """Emit the real `gfx_offsets.h`. Symbols not present in this variant
    fall through to the all-zero `_port_offset_stubs.h` catch-all (which
    uses `#ifndef` guards). The matching ROM build emits only the
    variant-specific symbols too.
    """
    lines = [
        "/* AUTO-GENERATED by tools/port/gen_host_assets.py. DO NOT EDIT. */",
        "/* Real `offset_*` macros for the configured TMC_GAME_VERSION. */",
        "",
        "#ifndef PORT_GENERATED_GFX_OFFSETS_H",
        "#define PORT_GENERATED_GFX_OFFSETS_H",
        "",
    ]
    seen: Set[str] = set()
    for asset in assets:
        sym = f"offset_{asset.symbol}"
        if sym in seen:
            continue
        seen.add(sym)
        lines.append(f"#define {sym} 0x{asset.block_offset:x}")
    lines += [
        "",
        "/* Catch-all zeros for any symbol not shipped in this variant. */",
        '#include "_port_offset_stubs.h"',
        "",
        "#endif /* PORT_GENERATED_GFX_OFFSETS_H */",
        "",
    ]
    out_path.write_text("\n".join(lines))


def build_palette_blob(assets: List[GfxAsset], baserom: bytes) -> Tuple[bytes, int]:
    """Produce the byte payload of `gGlobalGfxAndPalettes[]`.

    The block size is `max(asset.block_offset + asset.size)` rounded up
    to a 4-byte boundary -- that matches what the GBA build's
    `data/gfx/gfx_and_palettes.s` produces (a tightly-packed array of
    `.incbin`s, sequential by block offset). Each asset's bytes are
    copied from the baserom at its (variant-adjusted) file offset.
    Bytes that no asset claims are left zero.
    """
    total = 0
    for a in assets:
        end = a.block_offset + a.size
        if end > total:
            total = end
    # 4-byte align for parity with the GAS layout.
    if total % 4:
        total += 4 - (total % 4)
    blob = bytearray(total)
    for a in assets:
        if a.size == 0:
            continue
        end = a.file_offset + a.size
        if end > len(baserom):
            raise SystemExit(
                f"asset {a.symbol!r} reads {end} bytes from baserom "
                f"but baserom is only {len(baserom)} bytes"
            )
        blob[a.block_offset : a.block_offset + a.size] = baserom[
            a.file_offset : end
        ]
    return bytes(blob), total


def encode_gfx_unk0(item: GfxRaw, offsets: Dict[str, int]) -> int:
    """Pack `GfxRaw` into the 32-bit `unk0` field per the GAS macro.

    The .s macro is:
        .4byte (!terminator << 0x1F) + src + unknown * 0x1000000
    where `src` is `offset_<sym>` (a 24-bit byte offset into
    `gGlobalGfxAndPalettes`). The host's `LoadGfxGroup` consumes:
        ctrl       = (unk0 >> 24) & 0xF       /* the 'unknown' nibble */
        offset     = unk0 & 0xFFFFFF
        terminator = (unk0 >> 24) & 0x80      /* `loop continues` flag */
    """
    src_value = offsets.get(item.src_symbol, 0)
    if src_value < 0 or src_value >= (1 << 24):
        # Should not happen for well-formed gfx.json output, but guard
        # against silent corruption if a regenerated baserom drifts.
        raise SystemExit(
            f"`{item.src_symbol}` resolves to {src_value:#x}, "
            f"out of range for 24-bit gfx offset"
        )
    word = ((0 if item.terminator else 1) << 31) | (item.unknown << 24) | src_value
    return word & 0xFFFFFFFF


def encode_palette_byte3(item: PaletteSet) -> int:
    """Pack `(count, terminator)` into the high byte of the host
    `PaletteGroup.numPalettes`. The .s macro is:
        .byte offset, (count & 0xf) + (!terminator * 0x80)
    """
    return ((item.count & 0xF) | (0 if item.terminator else 0x80)) & 0xFF


def load_assets_json_blob(
    repo_root: Path, asset_path: str, variant: str
) -> Optional[Tuple[int, int]]:
    """Find a single ``path``-keyed entry in ``assets/assets.json`` and
    return ``(file_offset, size)`` for the active variant, or ``None``
    if the variant does not ship that asset.

    Honours the same conventions as :func:`load_gfx_assets`:

    * an ``offsets: { variant: int }`` block sets a running variant
      offset that adds to subsequent ``start`` values, until the next
      ``offsets`` block;
    * ``starts: { variant: int }`` (per-variant explicit starts) is
      taken verbatim and is *not* adjusted by the running variant
      offset (matches the gfx.json walker);
    * a ``variants`` filter restricts an entry to a subset of variants.
    """
    cfg_path = repo_root / "assets" / "assets.json"
    with cfg_path.open() as f:
        entries = json.load(f)

    variant_off = 0
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        if "offsets" in entry:
            variant_off = entry["offsets"].get(variant, 0)
            continue
        if entry.get("path") != asset_path:
            continue
        if "variants" in entry and variant not in entry["variants"]:
            continue
        if "starts" in entry:
            file_offset = entry["starts"].get(variant)
            if file_offset is None:
                continue
        elif "start" in entry:
            file_offset = entry["start"] + variant_off
        else:
            continue
        size = entry.get("size", 0)
        return (file_offset, size)
    return None


def write_assets_source(
    out_path: Path,
    blob: bytes,
    blob_size: int,
    parsed: ParsedAsmFile,
    offsets: Dict[str, int],
    frame_obj_lists: Optional[bytes] = None,
) -> None:
    """Emit `port_rom_assets.c`.

    Defines the three external symbols `gGlobalGfxAndPalettes[]`,
    `gGfxGroups[]`, and `gPaletteGroups[]`. Element types match the
    extern declarations in `src/common.c` and the `port_rom_data_types.h`
    typedefs (`GfxItem` and `PaletteGroup`).

    `port_rom_data_stubs.c` is excluded from the build when this file is
    compiled in (see CMakeLists.txt's TMC_BASEROM handling), so the
    strong host definitions here are the only ones the linker sees.
    """
    lines: List[str] = []
    lines.append(
        "/* AUTO-GENERATED by tools/port/gen_host_assets.py. DO NOT EDIT. */"
    )
    lines.append(
        "/* Strong host definitions of the gfx-table data symbols, populated"
    )
    lines.append(
        " * from the player's baserom. See `docs/sdl_port.md`'s `Game assets`"
    )
    lines.append(" * section for the wider context. */")
    lines.append("")
    lines.append("#ifdef __PORT__")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stddef.h>")
    lines.append("")
    lines.append('#include "port_rom_data_types.h"')
    lines.append("")

    # ---- gGlobalGfxAndPalettes -------------------------------------------
    lines.append(f"/* {blob_size} bytes, packed by block offset. */")
    lines.append(f"const uint8_t gGlobalGfxAndPalettes[{blob_size}] = {{")
    # Emit 16 bytes per row for compactness without dragging line-length
    # past ~80 columns.
    chunk = 16
    for i in range(0, len(blob), chunk):
        row = ", ".join(f"0x{b:02x}" for b in blob[i : i + chunk])
        comma = "," if i + chunk < len(blob) else ""
        lines.append(f"    {row}{comma}")
    lines.append("};")
    lines.append("")

    # ---- per-group GfxItem arrays ----------------------------------------
    for name in sorted(parsed.gfx_groups, key=lambda s: int(s.split("_")[-1])):
        items = parsed.gfx_groups[name]
        lines.append(f"static const GfxItem k_{name}[{len(items)}] = {{")
        for it in items:
            unk0 = encode_gfx_unk0(it, offsets)
            unk8 = (it.size & 0xFFFFFFFF) | (0x80000000 if it.compressed else 0)
            lines.append(
                f"    {{ .unk0 = {{ .raw = (int32_t)0x{unk0:08x} }}, "
                f".dest = 0x{it.dest:08x}u, .unk8 = 0x{unk8:08x}u }},"
            )
        lines.append("};")
    lines.append("")

    # ---- gGfxGroups[] ----------------------------------------------------
    n = len(parsed.gfx_index_order)
    lines.append(f"const GfxItem* gGfxGroups[{n}] = {{")
    for i, target in enumerate(parsed.gfx_index_order):
        if target is None:
            lines.append(f"    /* [{i:3d}] */ NULL,")
        else:
            lines.append(f"    /* [{i:3d}] */ &k_{target}[0],")
    lines.append("};")
    lines.append("")

    # ---- per-group PaletteGroup arrays -----------------------------------
    enum_table = parsed.enum_table
    for name in sorted(parsed.palette_groups, key=lambda s: int(s.split("_")[-1])):
        items = parsed.palette_groups[name]
        lines.append(f"static const PaletteGroup k_{name}[{len(items)}] = {{")
        for it in items:
            pid = enum_table.get(it.palette_expr)
            if pid is None:
                # Reference to an enum that the active variant didn't
                # declare. The matching ROM build would not assemble for
                # this variant either; emit zero so the table layout
                # stays stable and surface a clear error in CI.
                raise SystemExit(
                    f"palette_set palette={it.palette_expr}: enum unknown "
                    f"in this variant ({sorted(enum_table)[:3]}...)"
                )
            byte3 = encode_palette_byte3(it)
            lines.append(
                f"    {{ .paletteId = {pid}u, "
                f".destPaletteNum = {it.offset}u, "
                f".numPalettes = 0x{byte3:02x}u }},"
            )
        lines.append("};")
    lines.append("")

    # ---- gPaletteGroups[] ------------------------------------------------
    np = len(parsed.palette_index_order)
    lines.append(f"const PaletteGroup* gPaletteGroups[{np}] = {{")
    for i, target in enumerate(parsed.palette_index_order):
        if target is None:
            lines.append(f"    /* [{i:3d}] */ NULL,")
        else:
            lines.append(f"    /* [{i:3d}] */ &k_{target}[0],")
    lines.append("};")
    lines.append("")

    # ---- gFrameObjLists --------------------------------------------------
    # Two-level relative-offset blob consumed by `affine.c::DrawDirect`
    # via `port_oam_renderer.c::ram_DrawDirect`. The host build's
    # `port_unresolved_stubs.c` ships only a 256-byte zero placeholder
    # for this symbol, which makes every direct sprite emission (most
    # visibly title-screen "PRESS START" and the copyright "©" sprite)
    # walk into zeros and short-circuit. With the player's baserom in
    # hand we can extract the real blob and emit a strong host
    # definition that overrides the weak placeholder.
    if frame_obj_lists is not None:
        n = len(frame_obj_lists)
        lines.append(f"/* gFrameObjLists: {n} bytes from baserom (assets.json). */")
        lines.append(f"uint8_t gFrameObjLists[{n}] = {{")
        chunk = 16
        for i in range(0, n, chunk):
            row = ", ".join(f"0x{b:02x}" for b in frame_obj_lists[i : i + chunk])
            comma = "," if i + chunk < n else ""
            lines.append(f"    {row}{comma}")
        lines.append("};")
        lines.append("")

    lines.append("#endif /* __PORT__ */")
    lines.append("")

    out_path.write_text("\n".join(lines))


# ---------------------------------------------------------------------------
# Subcommands.
# ---------------------------------------------------------------------------


PLACEHOLDER_BASEROM_SIZE = 16 * 1024 * 1024  # 16,777,216 bytes


def cmd_make_placeholder_baserom(args: argparse.Namespace) -> None:
    """Write a deterministic all-zero baserom for testing.

    All-zero is the safest pattern for a placeholder baserom: the host
    `Port_LZ77UnComp*` reads a 4-byte header from `gGlobalGfxAndPalettes`
    and a zero header decodes as `out_size=0`, exiting the decompressor
    immediately without writing anything. Uncompressed `DmaSet` copies
    of zeros into VRAM/EWRAM are equally safe. So a build configured
    against this placeholder will still boot to a clean (black-screen)
    title state, which lets CI exercise the full asset-pipeline plumbing
    without shipping copyrighted ROM data.
    """
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    size = args.size if args.size is not None else PLACEHOLDER_BASEROM_SIZE
    with out.open("wb") as f:
        f.write(b"\x00" * size)
    print(f"wrote placeholder baserom: {out} ({size} bytes)")


def cmd_gen_offsets_stub(args: argparse.Namespace) -> None:
    """Regenerate the all-zero `_port_offset_stubs.h` catch-all.

    Normally invoked by CMake at configure time and written into the
    build directory (`${CMAKE_BINARY_DIR}/generated/port_offset_stubs/
    _port_offset_stubs.h`); `--out` selects the destination. When `--out`
    is omitted, falls back to the legacy in-tree location at
    `src/platform/shared/generated/assets/_port_offset_stubs.h` so manual
    invocations from the repo root still work.

    Each symbol gets `#ifndef ... #define X 0` so the real
    `gfx_offsets.h` (when present) can override it without #undef noise.

    Walks every JSON config under `assets/` (currently `gfx.json`,
    `map.json`, `assets.json`, `sounds.json`, `samples.json`) so that
    map / sound / data symbols stay covered alongside gfx -- the host
    `tmc_game_sources` library references symbols from all of them via
    `assets/{gfx,map}_offsets.h`.
    """
    repo_root = Path(args.repo_root or default_repo_root())
    if args.out:
        out_path = Path(args.out)
    else:
        out_path = (
            repo_root / "src" / "platform" / "shared" / "generated" / "assets"
            / "_port_offset_stubs.h"
        )
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Collect the union of every `offset_*` symbol across every JSON
    # config and every variant. (Same shape as the previously
    # hand-checked-in version of this file.) Each JSON has its own
    # `offsets` table, but for the purpose of cataloguing symbol
    # *names* we don't need to honour those -- name extraction is
    # offset-independent.
    symbols: Set[str] = set()
    json_paths = sorted((repo_root / "assets").glob("*.json"))
    for jp in json_paths:
        with jp.open() as f:
            entries = json.load(f)
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            # Most assets get their symbol from `path` (stripped of two
            # extensions, matching `BaseAsset::getSymbol()`); a few
            # entries -- notably the dungeon-map entries in `map.json`
            # -- override that with an explicit `name` field that the
            # asset_processor uses verbatim. Honour the override first.
            if "name" in entry:
                symbols.add(f"offset_{entry['name']}")
            elif "path" in entry:
                symbols.add(f"offset_{stem_twice(entry['path'])}")

    lines = [
        "/*",
        " * Auto-generated host-port stub for assets/*_offsets.h.",
        " *",
        " * Written at CMake configure time into",
        " *   ${CMAKE_BINARY_DIR}/generated/port_offset_stubs/_port_offset_stubs.h",
        " * by `tools/port/gen_host_assets.py gen-offsets-stub --out <path>`.",
        " * Do not commit a copy of this file -- it is `.gitignore`d for that",
        " * reason. Configure the SDL CMake project to regenerate it.",
        " *",
        " * The matching GBA ROM build (see Makefile / GBA.mk) writes the real",
        " * values to `assets/gfx_offsets.h` and `assets/map_offsets.h` from the",
        " * `asset_processor` tool after extracting the base ROM. The SDL host",
        " * port is decoupled from the ROM build and falls back to these",
        " * `#ifndef`-guarded zero stubs whenever the configure-time",
        " * `tools/port/gen_host_assets.py gen` did not produce a real value",
        " * (i.e. because the build was configured without a `-DTMC_BASEROM=`",
        " * path, or for symbol classes the generator does not yet cover --",
        " * map / sound / sample / data offsets, in this PR's scope).",
        " *",
        " * See docs/sdl_port.md (PR #2b.3, wave 3) for the wider context.",
        " */",
        "#ifndef PORT_GENERATED_ASSETS_OFFSET_STUBS_H",
        "#define PORT_GENERATED_ASSETS_OFFSET_STUBS_H",
        "",
    ]
    for sym in sorted(symbols):
        lines.append(f"#ifndef {sym}")
        lines.append(f"#define {sym} 0")
        lines.append("#endif")
    lines.append("")
    lines.append("#endif /* PORT_GENERATED_ASSETS_OFFSET_STUBS_H */")
    lines.append("")
    out_path.write_text("\n".join(lines))
    print(f"wrote {out_path} ({len(symbols)} symbols from {len(json_paths)} JSON configs)")


def cmd_gen(args: argparse.Namespace) -> None:
    repo_root = Path(args.repo_root or default_repo_root())
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    defines = variant_defines(args.variant)

    baserom = Path(args.baserom).read_bytes()

    # 1. assets/gfx.json -> assets + base offset.
    assets, _base = load_gfx_assets(repo_root, args.variant)

    # 2. parse the two .s files for the active variant.
    gfx_groups_s = repo_root / "data" / "gfx" / "gfx_groups.s"
    pal_groups_s = repo_root / "data" / "gfx" / "palette_groups.s"
    parsed_gfx = parse_asm_file(gfx_groups_s, defines)
    parsed_pal = parse_asm_file(pal_groups_s, defines)

    # Combine the parser outputs; only one of the two files defines each
    # of the index tables / per-group data sets, so straight-merge is OK.
    parsed = ParsedAsmFile(
        gfx_groups=parsed_gfx.gfx_groups,
        palette_groups=parsed_pal.palette_groups,
        gfx_index_order=parsed_gfx.gfx_index_order,
        palette_index_order=parsed_pal.palette_index_order,
        enum_table=parsed_pal.enum_table,
    )

    # 3. write the offsets header.
    write_offsets_header(out_dir / "gfx_offsets.h", assets)

    # 4. build the gGlobalGfxAndPalettes blob and emit the C source.
    blob, blob_size = build_palette_blob(assets, baserom)
    offset_table = {f"offset_{a.symbol}": a.block_offset for a in assets}

    # 5. extract the gFrameObjLists blob from baserom (if the variant
    # ships one) so the strong host definition overrides the 256-byte
    # zero placeholder in `port_unresolved_stubs.c`. Without this,
    # `DrawDirect` emissions (PRESS START, copyright ©, pause menu,
    # …) walk into zeros and never reach `gOAMControls.oam`.
    fol = load_assets_json_blob(repo_root, "gfx/gFrameObjLists.bin", args.variant)
    frame_obj_lists_bytes: Optional[bytes] = None
    if fol is not None:
        fol_off, fol_size = fol
        if fol_off + fol_size > len(baserom):
            raise SystemExit(
                f"gFrameObjLists reads {fol_off + fol_size} bytes from baserom "
                f"but baserom is only {len(baserom)} bytes"
            )
        frame_obj_lists_bytes = bytes(baserom[fol_off : fol_off + fol_size])

    write_assets_source(
        out_dir / "port_rom_assets.c",
        blob,
        blob_size,
        parsed,
        offset_table,
        frame_obj_lists=frame_obj_lists_bytes,
    )

    if args.verbose:
        fol_msg = (
            f" frame_obj_lists={len(frame_obj_lists_bytes)} bytes"
            if frame_obj_lists_bytes is not None
            else " frame_obj_lists=<absent>"
        )
        print(
            f"variant={args.variant} assets={len(assets)} "
            f"blob={blob_size} bytes "
            f"gfx_groups={len(parsed.gfx_groups)} ({len(parsed.gfx_index_order)} idx) "
            f"palette_groups={len(parsed.palette_groups)} ({len(parsed.palette_index_order)} idx) "
            f"enum_count={len(parsed.enum_table)}"
            f"{fol_msg}"
        )


def default_repo_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    g = sub.add_parser("gen", help="generate gfx_offsets.h + port_rom_assets.c")
    g.add_argument("--baserom", required=True, help="path to baserom.gba")
    g.add_argument("--variant", required=True, choices=ALL_VARIANTS)
    g.add_argument("--out-dir", required=True, help="output directory")
    g.add_argument("--repo-root", help="repository root (auto-detected)")
    g.add_argument("--verbose", action="store_true")
    g.set_defaults(func=cmd_gen)

    pl = sub.add_parser(
        "make-placeholder-baserom",
        help=f"write a deterministic {PLACEHOLDER_BASEROM_SIZE}-byte baserom for testing",
    )
    pl.add_argument("--out", required=True)
    pl.add_argument("--size", type=int, default=None)
    pl.set_defaults(func=cmd_make_placeholder_baserom)

    s = sub.add_parser(
        "gen-offsets-stub",
        help="(re)generate _port_offset_stubs.h (configure-time helper for CMake)",
    )
    s.add_argument("--repo-root", help="repository root (auto-detected)")
    s.add_argument(
        "--out",
        help=(
            "destination path for `_port_offset_stubs.h` (defaults to the "
            "legacy in-tree location for back-compat with manual invocations; "
            "CMake passes a path under ${CMAKE_BINARY_DIR}/generated/)"
        ),
    )
    s.set_defaults(func=cmd_gen_offsets_stub)

    args = p.parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
