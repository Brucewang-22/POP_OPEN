#!/usr/bin/env python3
"""
Generate C header from interactive/hardware.yaml.

This parser intentionally supports only a small YAML subset used here:
- two-level mapping
- scalar values: int, float, bool
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
    data = {}
    section = None

    for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue

        if not line.startswith(" "):
            if not line.endswith(":"):
                raise ValueError(f"{path}:{line_no}: top-level key must end with ':'")
            section = line[:-1].strip()
            data[section] = {}
            continue

        if section is None:
            raise ValueError(f"{path}:{line_no}: nested key without section")

        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if indent != 2:
            raise ValueError(f"{path}:{line_no}: nested key must use 2-space indentation")
        if ":" not in stripped:
            raise ValueError(f"{path}:{line_no}: invalid nested key format")

        key, value = stripped.split(":", 1)
        key = key.strip()
        if not key:
            raise ValueError(f"{path}:{line_no}: empty key")
        data[section][key] = _parse_scalar(value)

    return data


def as_c_bool(v):
    return "true" if bool(v) else "false"


def as_c_float(v):
    return f"{float(v):.6f}f"


def as_c_u(v):
    return f"{int(v)}U"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    input_path = pathlib.Path(args.input)
    out_path = pathlib.Path(args.output)
    cfg = parse_simple_yaml(input_path)

    def g(section, key):
        return cfg[section][key]

    text = f"""/* Auto-generated from {input_path.name}. Do not edit directly. */
#pragma once

#define HWCFG_MICROPHONE_MIN_FRAMES_1S {as_c_u(g("microphone", "min_frames_1s"))}
#define HWCFG_MICROPHONE_MIN_RMS_PERMILLE {as_c_u(g("microphone", "min_rms_permille"))}

#define HWCFG_TOUCH_MIN_ACTIVE_CHANNELS {as_c_u(g("touch", "min_active_channels"))}
#define HWCFG_TOUCH_EVENT_WINDOW_MS {as_c_u(g("touch", "event_window_ms"))}
#define HWCFG_TOUCH_MIN_EVENTS_IN_WINDOW {as_c_u(g("touch", "min_events_in_window"))}

#define HWCFG_IMU_REQUIRE_CALIBRATED {as_c_bool(g("imu", "require_calibrated"))}
#define HWCFG_IMU_ACCEL_NORM_MIN_MPS2 {as_c_float(g("imu", "accel_norm_min_mps2"))}
#define HWCFG_IMU_ACCEL_NORM_MAX_MPS2 {as_c_float(g("imu", "accel_norm_max_mps2"))}
#define HWCFG_IMU_MOTION_ACCEL_DELTA_MIN_MPS2 {as_c_float(g("imu", "motion_accel_delta_min_mps2"))}
#define HWCFG_IMU_MOTION_GYRO_AXIS_MIN_DPS {as_c_float(g("imu", "motion_gyro_axis_min_dps"))}
#define HWCFG_IMU_MOTION_MIN_CONSECUTIVE_HITS {as_c_u(g("imu", "motion_min_consecutive_hits"))}
"""

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())

