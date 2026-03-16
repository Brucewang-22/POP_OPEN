#!/bin/sh
[ -n "${BASH_VERSION:-}" ] || exec bash "$0" "$@"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
APP_DIR="${SCRIPT_DIR}"
CFG_FILE="${APP_DIR}/config/LIBTECH_POP_T5AI_DECONFIG.config"

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

echo "[INFO] 开始烧录"
tos.py flash "$@"

echo "[DONE] 编译与烧录完成"
