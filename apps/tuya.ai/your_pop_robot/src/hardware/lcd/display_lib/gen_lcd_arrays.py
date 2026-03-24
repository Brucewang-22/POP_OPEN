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


def generate_group(group_name: str, out_dir: Path, image_files: list[Path]) -> None:
    symbol_base = sanitize_symbol(group_name)
    symbol_ns = sanitize_symbol(f"{out_dir.parent.name}_{out_dir.name}")
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
    for path in sorted(image_files):
        data = path.read_bytes()
        sym = f"g_{symbol_ns}_asset_{sanitize_symbol(path.name)}"
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
    print(f"[lcd-assets] generated {out_h} and {out_c} ({len(image_files)} frames)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate embedded LCD image asset C files by eye group")
    parser.add_argument("--root", required=True)
    args = parser.parse_args()

    root = Path(args.root)
    groups: dict[tuple[str, str], list[Path]] = {}
    for image in sorted(root.glob("*/*/*.jpg")):
        if not image.is_file():
            continue
        if len(image.parts) < 3:
            continue
        group_name = f"{image.parent.parent.name}_{image.parent.name}"
        group_dir = str(image.parent)
        groups.setdefault((group_name, group_dir), []).append(image)

    if not groups:
        print("[lcd-assets] no jpg files found")
        return 0

    for (group_name, group_dir), files in sorted(groups.items()):
        generate_group(group_name, Path(group_dir), files)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
