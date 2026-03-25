#!/bin/sh
[ -n "${BASH_VERSION:-}" ] || exec bash "$0" "$@"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
APP_DIR="${SCRIPT_DIR}"
CFG_FILE="${APP_DIR}/config/LIBTECH_POP_T5AI_DECONFIG.config"
FLASH_PORT=""

for ((i=1; i<=$#; i++)); do
    arg="${!i}"
    case "${arg}" in
        -p|--port)
            next=$((i + 1))
            if (( next <= $# )); then
                FLASH_PORT="${!next}"
            fi
            ;;
        --port=*)
            FLASH_PORT="${arg#*=}"
            ;;
    esac
done

if [[ ! -f "${REPO_ROOT}/export.sh" ]]; then
    echo "[ERROR] 未找到 export.sh: ${REPO_ROOT}/export.sh"
    exit 1
fi

if [[ ! -f "${CFG_FILE}" ]]; then
    echo "[ERROR] 未找到配置文件: ${CFG_FILE}"
    exit 1
fi

cd "${REPO_ROOT}"
. ./export.sh

cd "${APP_DIR}"

echo "[INFO] 应用配置: ${CFG_FILE}"
cp "${CFG_FILE}" "${APP_DIR}/app_default.config"

if ! grep -q '^CONFIG_BOARD_CHOICE_T5AI=y$' "${APP_DIR}/app_default.config"; then
    echo "[ERROR] 当前配置未启用 T5AI 板型（CONFIG_BOARD_CHOICE_T5AI=y）"
    echo "[ERROR] 请检查: ${CFG_FILE}"
    exit 1
fi

echo "[INFO] 清理旧构建目录"
rm -rf .build

echo "[INFO] 开始编译"
tos.py build

if [[ -n "${FLASH_PORT}" ]]; then
    if [[ ! -e "${FLASH_PORT}" ]]; then
        echo "[ERROR] 指定烧录端口不存在: ${FLASH_PORT}"
        echo "[ERROR] 请确认开发板已连接并检查端口后重试。"
        exit 1
    fi
    echo "[INFO] 使用指定烧录端口: ${FLASH_PORT}"
else
    serial_ports=()
    for pattern in /dev/ttyACM* /dev/ttyUSB* /dev/cu.usb*; do
        for port in ${pattern}; do
            if [[ -e "${port}" ]]; then
                serial_ports+=("${port}")
            fi
        done
    done

    if (( ${#serial_ports[@]} == 0 )); then
        echo "[ERROR] 未检测到可用串口设备（/dev/ttyACM* 或 /dev/ttyUSB*）。"
        echo "[ERROR] 请先连接开发板，再执行烧录。"
        echo "[TIP] 也可手动指定端口：sh build_flash.sh -p /dev/ttyACM1"
        exit 1
    fi

    echo "[INFO] 检测到串口设备: ${serial_ports[*]}"
fi

echo "[INFO] 开始烧录"
tos.py flash "$@"

echo "[DONE] 编译与烧录完成"
