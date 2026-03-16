#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IMU 姿态 UI：串口解析 [IMU] roll/pitch/yaw（key=value），实时绘制 3D 立方体与数值。
与 POP_Open/demo/hardware/imu/imu_attitude_ui.py 相同的姿态解析与可视化逻辑。

用法: python3 imu_attitude_ui.py [串口] [波特率]
示例: python3 imu_attitude_ui.py /dev/ttyUSB0 460800
依赖: pip install -r requirements-imu-ui.txt
"""

import re
import sys
import time
import argparse
import math

try:
    import serial
except ImportError:
    print("请先安装 pyserial: pip install pyserial")
    sys.exit(1)
try:
    import matplotlib
    matplotlib.use("WebAgg")
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
except ImportError:
    print("请先安装 matplotlib: pip install matplotlib")
    sys.exit(1)
from mpl_toolkits.mplot3d import Axes3D, art3d  # noqa: F401

ANSI_STRIP = re.compile(r"\x1b\[[0-9;]*[a-zA-Zm]?")
KV_PATTERN = re.compile(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)")
DEFAULT_PORT = "/dev/ttyUSB0"
DEFAULT_BAUD = 460800
PLOT_INTERVAL_MS = 10
LINE_BUFFER_LIMIT = 4096


def strip_ansi(line):
    return ANSI_STRIP.sub("", line)


def parse_scalar(text):
    if isinstance(text, str) and text.lower().startswith("0x"):
        try:
            return int(text, 16)
        except ValueError:
            return text
    try:
        if any(ch in str(text) for ch in ".eE"):
            return float(text)
        return int(text)
    except ValueError:
        return text


def as_float(value):
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    return None


def euler_deg_to_quat(roll_deg, pitch_deg, yaw_deg):
    if roll_deg is None or pitch_deg is None or yaw_deg is None:
        return None
    r, p, y = math.radians(roll_deg), math.radians(pitch_deg), math.radians(yaw_deg)
    cr, sr = math.cos(r * 0.5), math.sin(r * 0.5)
    cp, sp = math.cos(p * 0.5), math.sin(p * 0.5)
    cy, sy = math.cos(y * 0.5), math.sin(y * 0.5)
    return (cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy)


def rotation_matrix_deg(roll_deg, pitch_deg, yaw_deg):
    r = math.radians(roll_deg or 0)
    p = math.radians(pitch_deg or 0)
    y = math.radians(yaw_deg or 0)
    cx, sx = math.cos(r), math.sin(r)
    cy, sy = math.cos(p), math.sin(p)
    cz, sz = math.cos(y), math.sin(y)
    return [
        [cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz],
        [cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz],
        [-sy, sx * cy, cx * cy],
    ]


def parse_imu_line(line):
    """解析 [IMU] 行，仅支持 key=value 一一对应（与 POP_Open/demo/hardware IMU 输出一致）。"""
    clean = strip_ansi(line).strip()
    if "[IMU]" not in clean:
        return None
    fields = {k.lower(): parse_scalar(v) for k, v in KV_PATTERN.findall(clean)}
    roll = as_float(fields.get("roll"))
    pitch = as_float(fields.get("pitch"))
    yaw = as_float(fields.get("yaw"))
    q = [as_float(fields.get("q0")), as_float(fields.get("q1")),
         as_float(fields.get("q2")), as_float(fields.get("q3"))]
    if None in q:
        q = euler_deg_to_quat(roll, pitch, yaw)
        q = list(q) if q else [None] * 4
    return {
        "roll": roll, "pitch": pitch, "yaw": yaw,
        "q0": q[0], "q1": q[1], "q2": q[2], "q3": q[3],
        "ax": as_float(fields.get("ax")),
        "ay": as_float(fields.get("ay")),
        "az": as_float(fields.get("az")),
        "anorm": as_float(fields.get("anorm")),
        "gx": as_float(fields.get("gx")),
        "gy": as_float(fields.get("gy")),
        "gz": as_float(fields.get("gz")),
        "who": fields.get("who"),
        "temp": as_float(fields.get("temp")),
        "samples": fields.get("samples"),
        "calibrated": int(fields.get("calibrated", 0)) if fields.get("calibrated") is not None else None,
    }


def cube_face_indices():
    """Cube 8 vertices, 6 faces (each face 4 vertex indices)."""
    return [
        [0, 1, 2, 3],  # back z-
        [4, 5, 6, 7],  # front z+
        [0, 1, 5, 4],  # bottom y-
        [2, 3, 7, 6],  # top y+
        [0, 3, 7, 4],  # left x-
        [1, 2, 6, 5],  # right x+
    ]


def cube_verts(half=0.5):
    h = half
    return [
        (-h, -h, -h), (h, -h, -h), (h, h, -h), (-h, h, -h),
        (-h, -h, h), (h, -h, h), (h, h, h), (-h, h, h),
    ]


def main():
    parser = argparse.ArgumentParser(description="IMU 姿态（3D 立方体 + 数值），your_pop_robot")
    parser.add_argument("port", nargs="?", default=DEFAULT_PORT, help="串口")
    parser.add_argument("baud", nargs="?", type=int, default=DEFAULT_BAUD, help="波特率")
    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.01)
    except Exception as e:
        print(f"打开串口失败: {args.port} @ {args.baud}: {e}")
        sys.exit(1)
    print("已打开", args.port, "@", args.baud, "，等待 [IMU]… 姿态在浏览器中显示。")

    line_buf = bytearray()
    first_data_logged = [False]
    latest = [None]

    fig = plt.figure(figsize=(10, 6))
    ax3 = fig.add_subplot(121, projection="3d")
    ax2 = fig.add_subplot(122)
    ax2.set_axis_off()

    verts_orig = cube_verts()
    face_indices = cube_face_indices()
    face_colors = ["#1565C0"] * 6
    poly = art3d.Poly3DCollection([], facecolors=face_colors, edgecolors="#0D47A1", linewidths=1.2, alpha=1.0)
    axis_arrows = [None, None, None]
    axis_labels = [None, None, None]
    arrow_len = 1.10
    ax3.add_collection3d(poly)
    ax3.set_xlim(-0.95, 0.95)
    ax3.set_ylim(-0.95, 0.95)
    ax3.set_zlim(-0.95, 0.95)
    ax3.set_box_aspect([1.0, 1.0, 1.0])
    ax3.set_axis_off()

    text_block = ax2.text(0.08, 0.96, "", transform=ax2.transAxes, fontsize=12,
                          verticalalignment="top", fontfamily="monospace",
                          bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.25),
                          linespacing=1.6)

    def read_serial():
        n = ser.in_waiting
        if n > 0:
            line_buf.extend(ser.read(n))
        while True:
            idx = line_buf.find(b"\n")
            if idx == -1:
                idx = line_buf.find(b"\r")
            if idx == -1:
                if len(line_buf) > LINE_BUFFER_LIMIT:
                    line_buf.clear()
                break
            raw = bytes(line_buf[: idx + 1])
            del line_buf[: idx + 1]
            try:
                line = raw.decode("utf-8", errors="ignore").strip()
            except Exception:
                continue
            if not line:
                continue
            parsed = parse_imu_line(line)
            if parsed:
                latest[0] = parsed
                if not first_data_logged[0]:
                    first_data_logged[0] = True
                    print("已收到 [IMU] 数据。")

    def fmt(v, decimals=2, suffix=""):
        if v is None:
            return "--"
        if isinstance(v, float):
            return f"{v:.{decimals}f}{suffix}"
        return f"{v}{suffix}"

    title_set = [False]

    def draw_cube(roll_deg, pitch_deg, yaw_deg):
        R = rotation_matrix_deg(roll_deg, pitch_deg, yaw_deg)
        rotated = []
        for v in verts_orig:
            x = R[0][0] * v[0] + R[0][1] * v[1] + R[0][2] * v[2]
            y_ = R[1][0] * v[0] + R[1][1] * v[1] + R[1][2] * v[2]
            z = R[2][0] * v[0] + R[2][1] * v[1] + R[2][2] * v[2]
            rotated.append([x, y_, z])
        faces = []
        for idx in face_indices:
            face = [rotated[i] for i in idx]
            faces.append(face)
        poly.set_verts(faces)

        axes_world = [
            (R[0][0] * arrow_len, R[1][0] * arrow_len, R[2][0] * arrow_len),
            (R[0][1] * arrow_len, R[1][1] * arrow_len, R[2][1] * arrow_len),
            (R[0][2] * arrow_len, R[1][2] * arrow_len, R[2][2] * arrow_len),
        ]
        colors = ["#E53935", "#43A047", "#1E88E5"]

        for i in range(3):
            if axis_arrows[i] is not None:
                axis_arrows[i].remove()
            if axis_labels[i] is not None:
                axis_labels[i].remove()
            dx, dy, dz = axes_world[i]
            axis_arrows[i] = ax3.quiver(
                0.0, 0.0, 0.0, dx, dy, dz,
                color=colors[i], linewidth=3.0, arrow_length_ratio=0.18,
            )
            label_text = "XYZ"[i]
            axis_labels[i] = ax3.text(
                dx * 1.08, dy * 1.08, dz * 1.08, label_text,
                color=colors[i], fontsize=12, fontweight="bold", ha="center", va="center",
            )

    def update(_):
        if not title_set[0] and getattr(fig.canvas, "manager", None) is not None:
            try:
                fig.canvas.manager.set_window_title("IMU Attitude (your_pop_robot)")
                title_set[0] = True
            except Exception:
                pass
        read_serial()
        d = latest[0]
        if d is None:
            draw_cube(0, 0, 0)
            text_block.set_text(
                "  ——— Attitude ———\n"
                "    Roll   Pitch   Yaw\n"
                "    --     --      --\n\n"
                "  ——— Quaternion ———\n"
                "    q0  q1  q2  q3\n"
                "    --  --  --  --\n\n"
                "  ——— Accelerometer (m/s^2) ———\n"
                "    Ax     Ay     Az\n"
                "    --     --     --\n\n"
                "  ——— Gyroscope (dps) ———\n"
                "    Gx     Gy     Gz\n"
                "    --     --     --\n\n"
                "  ——— Status ———\n"
                "    WHO    temp   samples  calibrated  anorm(m/s²)\n"
                "    --     --     --       --          --\n\n"
                "  (等待 [IMU] 串口数据...)"
            )
            return

        r, p, y = d.get("roll"), d.get("pitch"), d.get("yaw")
        draw_cube(r, p, y)

        who_s = f"0x{d['who']:02X}" if isinstance(d.get("who"), int) else fmt(d.get("who"))
        cal_s = "--" if d.get("calibrated") is None else ("YES" if d.get("calibrated") else "NO")
        text_block.set_text(
            "  ——— Attitude ———\n"
            f"    Roll   Pitch   Yaw\n"
            f"    {fmt(r):>6} {fmt(p):>6} {fmt(y):>6}\n\n"
            "  ——— Quaternion ———\n"
            f"    q0     q1     q2     q3\n"
            f"    {fmt(d.get('q0'), 4)}  {fmt(d.get('q1'), 4)}  {fmt(d.get('q2'), 4)}  {fmt(d.get('q3'), 4)}\n\n"
            "  ——— Accelerometer (m/s^2) ———\n"
            f"    Ax     Ay     Az\n"
            f"    {fmt(d.get('ax'), 4)}  {fmt(d.get('ay'), 4)}  {fmt(d.get('az'), 4)}\n\n"
            "  ——— Gyroscope (dps) ———\n"
            f"    Gx     Gy     Gz\n"
            f"    {fmt(d.get('gx'), 3)}  {fmt(d.get('gy'), 3)}  {fmt(d.get('gz'), 3)}\n\n"
            "  ——— Status ———\n"
            f"    WHO    temp   samples  calibrated  anorm(m/s²)\n"
            f"    {who_s:>4}   {fmt(d.get('temp'), 2)} C   {fmt(d.get('samples')):>6}   {cal_s:>10}   {fmt(d.get('anorm'), 4)}"
        )

    ani = FuncAnimation(fig, update, interval=PLOT_INTERVAL_MS, blit=False)
    plt.tight_layout()
    plt.show()
    ser.close()


if __name__ == "__main__":
    main()
