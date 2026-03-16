#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
麦克风波形 UI：串口解析 [MIC] rms，实时绘制。

用法: python3 mic_waveform_ui.py [串口] [波特率]
依赖: pip install -r requirements-mic-ui.txt
"""

import re
import sys
import time
import argparse
from collections import deque

try:
    import serial
except ImportError:
    print("请先安装 pyserial: pip install pyserial")
    sys.exit(1)
try:
    import tornado.web
except ImportError:
    print("请执行: pip install tornado")
    sys.exit(1)
try:
    import matplotlib

    matplotlib.use("WebAgg")
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
except ImportError:
    print("请先安装: pip install -r requirements-mic-ui.txt")
    sys.exit(1)

MIC_PATTERN = re.compile(r"\[MIC\]\s*frames_1s=(\d+)\s+rms=(\d+)\s+status=(\w+)")
ANSI_STRIP = re.compile(r"\x1b\[[0-9;]*[a-zA-Zm]?")
DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = 460800
HISTORY_SEC = 60
PLOT_INTERVAL_MS = 30


def strip_ansi(line):
    return ANSI_STRIP.sub("", line)


def parse_mic_line(line):
    line = strip_ansi(line)
    m = MIC_PATTERN.search(line)
    if m:
        return int(m.group(1)), int(m.group(2)), m.group(3)
    return None


def main():
    parser = argparse.ArgumentParser(description="麦克风 rms 波形")
    parser.add_argument("port", nargs="?", default=DEFAULT_PORT, help="串口")
    parser.add_argument("baud", nargs="?", type=int, default=DEFAULT_BAUD, help="波特率")
    parser.add_argument("--history", type=int, default=HISTORY_SEC, help="显示最近秒数")
    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.05)
    except Exception as e:
        print(f"打开串口失败: {args.port} @ {args.baud}: {e}")
        sys.exit(1)
    print(f"已打开 {args.port} @ {args.baud}，等待 [MIC] 日志...")

    times = deque(maxlen=args.history * 50)
    rms_values = deque(maxlen=args.history * 50)
    start_t = time.time()
    first_data_logged = [False]
    line_buf = bytearray()

    fig, ax_rms = plt.subplots(1, 1, figsize=(10, 4))
    fig.suptitle("Microphone Level (RMS)")
    ax_rms.set_ylabel("rms (0~1000)")
    ax_rms.set_xlabel("time (s 0=earliest window)")
    ax_rms.set_ylim(0, 1050)
    ax_rms.grid(True, alpha=0.3)
    line_rms, = ax_rms.plot([], [], "b-", lw=1.5, label="rms")
    ax_rms.legend(loc="upper right")

    def read_serial():
        n = ser.in_waiting
        if n > 0:
            line_buf.extend(ser.read(n))
        while True:
            idx = line_buf.find(b"\n")
            if idx == -1:
                idx = line_buf.find(b"\r")
            if idx == -1:
                if len(line_buf) > 4096:
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
            parsed = parse_mic_line(line)
            if parsed:
                _, rms, _ = parsed
                t = time.time() - start_t
                times.append(t)
                rms_values.append(rms)
                if not first_data_logged[0]:
                    first_data_logged[0] = True
                    print("已收到 [MIC] 数据。")

    title_set = [False]

    def update(_):
        if not title_set[0] and getattr(fig.canvas, "manager", None) is not None:
            try:
                fig.canvas.manager.set_window_title("microphone")
                title_set[0] = True
            except Exception:
                pass
        read_serial()
        if not times:
            return
        t_min = times[-1] - args.history if times[-1] > args.history else times[0]
        x_rel = [t - t_min for t in times]
        line_rms.set_data(x_rel, list(rms_values))
        ax_rms.set_xlim(0, args.history)

    _ani = FuncAnimation(fig, update, interval=PLOT_INTERVAL_MS, blit=False, cache_frame_data=False)
    plt.tight_layout()
    plt.show()
    ser.close()


if __name__ == "__main__":
    main()

