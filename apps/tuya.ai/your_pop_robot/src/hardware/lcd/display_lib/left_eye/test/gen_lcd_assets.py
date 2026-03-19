#!/usr/bin/env python3
import argparse
import os
from pathlib import Path


def sanitize_symbol(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch)
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate embedded LCD JPG asset C files")
    parser.add_argument("--out-c", required=True)
    parser.add_argument("--out-h", required=True)
    parser.add_argument("--inputs", nargs="+", required=True)
    args = parser.parse_args()

    input_paths = sorted(Path(p) for p in args.inputs if Path(p).is_file())

    out_c = Path(args.out_c)
    out_h = Path(args.out_h)
    out_c.parent.mkdir(parents=True, exist_ok=True)
    out_h.parent.mkdir(parents=True, exist_ok=True)

    guard = "LOCAL_LCD_ASSETS_H"
    h_lines = [
        "#ifndef " + guard,
        "#define " + guard,
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    const uint8_t *data;",
        "    uint32_t len;",
        "    const char *name;",
        "} LOCAL_LCD_FRAME_T;",
        "",
        "extern const LOCAL_LCD_FRAME_T g_local_lcd_frames[];",
        "extern const uint32_t g_local_lcd_frame_count;",
        "",
        "#endif",
        "",
    ]
    out_h.write_text("\n".join(h_lines), encoding="utf-8")

    c_lines = [
        '#include "local_lcd_assets.h"',
        "",
    ]

    entries = []
    for path in input_paths:
        data = path.read_bytes()
        sym = "g_lcd_asset_" + sanitize_symbol(path.name)
        c_lines.append(f"static const uint8_t {sym}[] = {{")
        c_lines.append(bytes_to_c_array(data))
        c_lines.append("};")
        c_lines.append("")
        entries.append((sym, len(data), path.name))

    c_lines.append("const LOCAL_LCD_FRAME_T g_local_lcd_frames[] = {")
    for sym, length, name in entries:
        c_lines.append(f'    {{ {sym}, {length}u, "{name}" }},')
    c_lines.append("};")
    c_lines.append("")
    c_lines.append(
        "const uint32_t g_local_lcd_frame_count = (uint32_t)(sizeof(g_local_lcd_frames) / sizeof(g_local_lcd_frames[0]));"
    )
    c_lines.append("")

    out_c.write_text("\n".join(c_lines), encoding="utf-8")
    print(f"[lcd-assets] generated {out_h} and {out_c} ({len(input_paths)} frames)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
