#!/usr/bin/env python3
import argparse
from pathlib import Path


def sanitize_symbol(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch.lower())
        else:
            out.append("_")
    s = "".join(out)
    if s and s[0].isdigit():
        s = "_" + s
    return s


def bytes_to_c_array(data: bytes, width: int = 12) -> str:
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i : i + width]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    return "\n".join(lines)


def generate_group(group_name: str, jpg_files: list[Path]) -> None:
    symbol_base = sanitize_symbol(group_name)
    out_dir = jpg_files[0].parent
    out_h = out_dir / f"{symbol_base}_lcd_assets.h"
    out_c = out_dir / f"{symbol_base}_lcd_assets.c"
    guard = f"{symbol_base.upper()}_LCD_ASSETS_H"

    h_lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "lcd_asset_types.h"',
        "",
        f"extern const LOCAL_LCD_FRAME_T g_{symbol_base}_lcd_frames[];",
        f"extern const uint32_t g_{symbol_base}_lcd_frame_count;",
        "",
        "#endif",
        "",
    ]
    out_h.write_text("\n".join(h_lines), encoding="utf-8")

    c_lines = [
        f'#include "{symbol_base}_lcd_assets.h"',
        "",
    ]
    entries = []
    for path in sorted(jpg_files):
        data = path.read_bytes()
        sym = f"g_{symbol_base}_asset_{sanitize_symbol(path.name)}"
        c_lines.append(f"static const uint8_t {sym}[] = {{")
        c_lines.append(bytes_to_c_array(data))
        c_lines.append("};")
        c_lines.append("")
        entries.append((sym, len(data), path.name))

    c_lines.append(f"const LOCAL_LCD_FRAME_T g_{symbol_base}_lcd_frames[] = {{")
    for sym, length, name in entries:
        c_lines.append(f'    {{ {sym}, {length}u, "{name}" }},')
    c_lines.append("};")
    c_lines.append("")
    c_lines.append(
        f"const uint32_t g_{symbol_base}_lcd_frame_count = "
        f"(uint32_t)(sizeof(g_{symbol_base}_lcd_frames) / sizeof(g_{symbol_base}_lcd_frames[0]));"
    )
    c_lines.append("")
    out_c.write_text("\n".join(c_lines), encoding="utf-8")
    print(f"[lcd-assets] generated {out_h} and {out_c} ({len(jpg_files)} frames)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate embedded LCD JPG asset C files by eye group")
    parser.add_argument("--root", required=True)
    args = parser.parse_args()

    root = Path(args.root)
    groups: dict[str, list[Path]] = {}

    for jpg in sorted(root.glob("*/*/*.jpg")):
        if not jpg.is_file():
            continue
        if len(jpg.parts) < 3:
            continue
        group_name = jpg.parent.parent.name
        groups.setdefault(group_name, []).append(jpg)

    if not groups:
        print("[lcd-assets] no jpg files found")
        return 0

    for group_name, files in sorted(groups.items()):
        generate_group(group_name, files)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
