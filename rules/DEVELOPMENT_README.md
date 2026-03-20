# DEVELOPMENT_README

## 开发环境初始化
```bash
cd ~/Documents/TuyaOpen
. ./export.sh
```

## 选择板型
```bash
tos.py config choice
```

## 清理构建目录
```bash
rm -rf .build
```

## 编译与烧录
- 编译并烧录：
```bash
sh build_flash.sh
```

- 只编译：
```bash
tos.py build
```

- 只烧录：
```bash
tos.py flash
```

## 业务开发入口
进入 `apps/tuya.ai/your_pop_robot/`，按照该目录下 `README.md` 说明进行开发。

## 调试与烧录说明
烧录串口使用 ACM0 波特率 921600
调试串口使用 ACM1 波特率 460800

特别注意：调试过程中ACM1 端口可能会由于上一步串口资源释放不干净导致被占用，此时串口号向后延排为 ACM2 ACM3 等
调试指令：python -m serial.tools.miniterm /dev/ttyACM1 460800