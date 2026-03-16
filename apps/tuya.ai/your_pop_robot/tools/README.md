# your_pop_robot 工具

## IMU 姿态可视化（imu_attitude_ui.py）

与 **POP_Open/demo/hardware/imu/imu_attitude_ui.py** 相同的姿态解析与展示逻辑：从设备串口读取包含 `[IMU]` 的 key=value 行，实时绘制 3D 立方体与姿态/加速度/角速度/状态数值。

### 依赖

```bash
pip install -r requirements-imu-ui.txt
```

（pyserial、matplotlib）

### 用法

1. 设备烧录 your_pop_robot 固件，串口输出与波特率与编译配置一致（如 460800）。
2. 设备与 PC 通过串口连接（如 `/dev/ttyUSB0` 或 Windows `COM3`）。
3. 在工程根目录或本 `tools` 目录下执行：

```bash
python3 imu_attitude_ui.py [串口] [波特率]
```

示例：

```bash
python3 imu_attitude_ui.py /dev/ttyUSB0 460800
```

4. 若固件已输出 `[IMU] roll=... pitch=... yaw=...` 等行，脚本会打开 matplotlib 窗口：左侧 3D 立方体随姿态旋转，右侧为数值面板。使用 **matplotlib WebAgg** 时，浏览器中会打开交互窗口。

### 固件输出格式（一一对应）

固件周期打印一行，与 POP_Open/demo/hardware IMU 一致，仅此一种 key=value 格式：

```
[IMU] who=0x68 roll=... pitch=... yaw=... temp=... samples=... calibrated=... ax=... ay=... az=... anorm=... gx=... gy=... gz=...
```

脚本只解析上述 key，无冗余兼容。

## 麦克风波形可视化（mic_waveform_ui.py）

参考 `template/demo/hardware/microphone/mic_waveform_ui.py`，从串口解析 `[MIC] frames_1s=... rms=... status=...`，实时显示 `rms` 波形。

### 依赖

```bash
pip install -r requirements-mic-ui.txt
```

### 用法

```bash
python3 mic_waveform_ui.py [串口] [波特率]
```

示例：

```bash
python3 mic_waveform_ui.py /dev/ttyACM0 460800
```

### 固件输出格式

```
[MIC] frames_1s=50 rms=123 status=OK
```
