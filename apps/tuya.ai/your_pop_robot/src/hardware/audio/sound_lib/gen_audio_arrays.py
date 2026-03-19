#!/usr/bin/env python3
import argparse
from pathlib import Path


def sanitize_symbol(name: str) -> str:
    chars = []
    for ch in name:
        if ch.isalnum():
            chars.append(ch)
        else:
            chars.append("_")
    symbol = "".join(chars)
    if symbol and symbol[0].isdigit():
        symbol = "_" + symbol
    return symbol


def bytes_to_c_array(data: bytes, width: int = 12) -> str:
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i : i + width]
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    return "\n".join(lines)


def generate_for_directory(audio_dir: Path, mp3_files: list[Path]) -> None:
    folder_name = audio_dir.name
    base_name = f"{sanitize_symbol(folder_name)}_audio_assets"
    header_name = f"{base_name}.h"
    source_name = f"{base_name}.c"
    header_path = audio_dir / header_name
    source_path = audio_dir / source_name
    include_guard = sanitize_symbol(header_name).upper()

    header_lines = [
        f"#ifndef {include_guard}",
        f"#define {include_guard}",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    const uint8_t *data;",
        "    uint32_t len;",
        "    const char *name;",
        "} LOCAL_AUDIO_CLIP_T;",
        "",
        f"extern const LOCAL_AUDIO_CLIP_T g_{sanitize_symbol(folder_name)}_audio_clips[];",
        f"extern const uint32_t g_{sanitize_symbol(folder_name)}_audio_clip_count;",
        "",
        "#endif",
        "",
    ]

    source_lines = [
        f'#include "{header_name}"',
        "",
    ]

    entries = []
    for mp3_path in sorted(mp3_files):
        data = mp3_path.read_bytes()
        symbol = f"g_{sanitize_symbol(folder_name)}_{sanitize_symbol(mp3_path.stem)}_mp3"
        source_lines.append(f"static const uint8_t {symbol}[] = {{")
        source_lines.append(bytes_to_c_array(data))
        source_lines.append("};")
        source_lines.append("")
        entries.append((symbol, len(data), mp3_path.name))

    array_name = f"g_{sanitize_symbol(folder_name)}_audio_clips"
    count_name = f"g_{sanitize_symbol(folder_name)}_audio_clip_count"
    source_lines.append(f"const LOCAL_AUDIO_CLIP_T {array_name}[] = {{")
    for symbol, length, file_name in entries:
        source_lines.append(f'    {{ {symbol}, {length}u, "{file_name}" }},')
    source_lines.append("};")
    source_lines.append("")
    source_lines.append(
        f"const uint32_t {count_name} = (uint32_t)(sizeof({array_name}) / sizeof({array_name}[0]));"
    )
    source_lines.append("")

    header_path.write_text("\n".join(header_lines), encoding="utf-8")
    source_path.write_text("\n".join(source_lines), encoding="utf-8")

    print(f"[audio-arrays] generated {header_path} and {source_path} ({len(entries)} clips)")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Recursively convert MP3 files under sound_lib subdirectories into C arrays."
    )
    parser.add_argument(
        "--root",
        default=Path(__file__).resolve().parent,
        type=Path,
        help="sound_lib root directory",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    if not root.is_dir():
        raise SystemExit(f"root directory not found: {root}")

    groups: dict[Path, list[Path]] = {}
    for mp3_path in sorted(root.rglob("*.mp3")):
        if mp3_path.parent == root:
            continue
        groups.setdefault(mp3_path.parent, []).append(mp3_path)

    if not groups:
        print(f"[audio-arrays] no mp3 found under {root}")
        return 0

    for audio_dir, mp3_files in groups.items():
        generate_for_directory(audio_dir, mp3_files)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
