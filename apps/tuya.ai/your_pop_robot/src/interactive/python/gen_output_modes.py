#!/usr/bin/env python3
"""
Generate output mode config header from interactive/yaml/output.yaml.
"""

import argparse
import pathlib
import re
import sys


def _parse_scalar(raw: str):
    val = raw.strip()
    if val.lower() == "true":
        return True
    if val.lower() == "false":
        return False
    if re.fullmatch(r"[+-]?\d+", val):
        return int(val, 10)
    if re.fullmatch(r"[+-]?(\d+\.\d*|\d*\.\d+)", val):
        return float(val)
    return val


def parse_simple_yaml(path: pathlib.Path):
    root = {}
    stack = [(-1, root)]

    for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue

        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if indent % 2 != 0:
            raise ValueError(f"{path}:{line_no}: indentation must be 2-space aligned")
        if ":" not in stripped:
            raise ValueError(f"{path}:{line_no}: missing ':'")

        key, value = stripped.split(":", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            raise ValueError(f"{path}:{line_no}: empty key")

        while stack and indent <= stack[-1][0]:
            stack.pop()
        if not stack:
            raise ValueError(f"{path}:{line_no}: invalid indentation")

        current = stack[-1][1]
        if value == "":
            node = {}
            current[key] = node
            stack.append((indent, node))
        else:
            current[key] = _parse_scalar(value)

    return root


def as_c_bool(v):
    return "true" if bool(v) else "false"


def as_c_u(v):
    return f"{int(v)}U"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    input_path = pathlib.Path(args.input)
    output_path = pathlib.Path(args.output)
    cfg = parse_simple_yaml(input_path)

    def g(*keys):
        node = cfg
        for k in keys:
            node = node[k]
        return node

    text = f"""/* Auto-generated from {input_path.name}. Do not edit directly. */
#pragma once

#define HWCFG_LCD_MODEL1_LOOP_TIME_MS {as_c_u(g("lcd", "model1", "loop_time_ms"))}
#define HWCFG_LCD_MODEL1_FPS {as_c_u(g("lcd", "model1", "fps"))}
#define HWCFG_LCD_MODEL2_LOOP_TIME_MS {as_c_u(g("lcd", "model2", "loop_time_ms"))}
#define HWCFG_LCD_MODEL2_FPS {as_c_u(g("lcd", "model2", "fps"))}
#define HWCFG_LCD_MODEL3_LOOP_TIME_MS {as_c_u(g("lcd", "model3", "loop_time_ms"))}
#define HWCFG_LCD_MODEL3_FPS {as_c_u(g("lcd", "model3", "fps"))}

#define HWCFG_AUDIO_MODEL1_VOLUME {as_c_u(g("audio", "model1", "volume"))}
#define HWCFG_AUDIO_MODEL2_VOLUME {as_c_u(g("audio", "model2", "volume"))}
#define HWCFG_AUDIO_MODEL3_VOLUME {as_c_u(g("audio", "model3", "volume"))}

#define HWCFG_MOTO_MODEL1_ANGLE_DEG {as_c_u(g("moto", "model1", "angle_deg"))}
#define HWCFG_MOTO_MODEL1_SPEED_DPS {as_c_u(g("moto", "model1", "speed_dps"))}
#define HWCFG_MOTO_MODEL1_LOOP {as_c_bool(g("moto", "model1", "loop"))}
#define HWCFG_MOTO_MODEL1_HOLD_TORQUE {as_c_bool(g("moto", "model1", "hold_torque"))}
#define HWCFG_MOTO_MODEL1_RUN_TIME_MS {as_c_u(g("moto", "model1", "run_time_ms"))}

#define HWCFG_MOTO_MODEL2_ANGLE_DEG {as_c_u(g("moto", "model2", "angle_deg"))}
#define HWCFG_MOTO_MODEL2_SPEED_DPS {as_c_u(g("moto", "model2", "speed_dps"))}
#define HWCFG_MOTO_MODEL2_LOOP {as_c_bool(g("moto", "model2", "loop"))}
#define HWCFG_MOTO_MODEL2_HOLD_TORQUE {as_c_bool(g("moto", "model2", "hold_torque"))}
#define HWCFG_MOTO_MODEL2_RUN_TIME_MS {as_c_u(g("moto", "model2", "run_time_ms"))}

#define HWCFG_MOTO_MODEL3_ANGLE_DEG {as_c_u(g("moto", "model3", "angle_deg"))}
#define HWCFG_MOTO_MODEL3_SPEED_DPS {as_c_u(g("moto", "model3", "speed_dps"))}
#define HWCFG_MOTO_MODEL3_LOOP {as_c_bool(g("moto", "model3", "loop"))}
#define HWCFG_MOTO_MODEL3_HOLD_TORQUE {as_c_bool(g("moto", "model3", "hold_torque"))}
#define HWCFG_MOTO_MODEL3_RUN_TIME_MS {as_c_u(g("moto", "model3", "run_time_ms"))}
"""

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())

