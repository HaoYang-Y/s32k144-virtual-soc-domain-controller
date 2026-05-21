#!/bin/bash
# =====================================================================
# @brief MCU 端一键交叉编译 + J-Link 烧录脚本
#        编译指定 S32K144 驱动模块并烧录到开发板
#
# @usage ./tools/mcu_flash.sh <module> [flash]
#         <module> - gpio|uart|timer|adc|flexcan|clock
#         [flash]  - 可选，加上则编译后烧录，不加仅编译
#
# @example
#   ./tools/mcu_flash.sh gpio        # 仅编译 GPIO 模块
#   ./tools/mcu_flash.sh gpio flash  # 编译并烧录 GPIO 模块
# =====================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
MCU_DIR="${PROJECT_DIR}/mcu"

# 有效模块列表
VALID_MODULES=("gpio" "uart" "timer" "adc" "flexcan" "clock")

# 显示使用说明
usage() {
    echo "用法: $0 <module> [flash]"
    echo "  <module> - $(IFS='|'; echo "${VALID_MODULES[*]}")"
    echo "  [flash]  - 可选，加上则编译后烧录，不加仅编译"
    echo ""
    echo "示例:"
    echo "  $0 gpio            # 仅编译 GPIO"
    echo "  $0 gpio flash      # 编译并烧录 GPIO"
    exit 1
}

# 校验模块名
validate_module() {
    local module="$1"
    for m in "${VALID_MODULES[@]}"; do
        if [ "${m}" = "${module}" ]; then
            return 0
        fi
    done
    echo "[ERROR] 无效模块: ${module}"
    usage
}

# 获取模块对应的目标文件名
get_target_name() {
    local module="$1"
    case "${module}" in
        gpio)    echo "gpio_demo" ;;
        uart)    echo "uart_demo" ;;
        timer)   echo "timer_demo" ;;
        adc)     echo "adc_demo" ;;
        flexcan) echo "flexcan_demo" ;;
        clock)   echo "clock_demo" ;;
    esac
}

# ----- 主流程 -----

# 检查参数
if [ $# -lt 1 ]; then
    usage
fi

MODULE="$1"
ACTION="${2:-build}"  # 默认为仅编译

validate_module "${MODULE}"
TARGET_NAME="$(get_target_name "${MODULE}")"

MODULE_DIR="${MCU_DIR}/${MODULE}"
TARGET_HEX="${MODULE_DIR}/${TARGET_NAME}.hex"

echo "========================================="
echo " MCU 模块: ${MODULE}"
echo " 目标文件: ${TARGET_NAME}"
echo " 操作模式: $([ "${ACTION}" = "flash" ] && echo '编译 + 烧录' || echo '仅编译')"
echo "========================================="

# 第一步：交叉编译
echo ""
echo "[BUILD] 开始编译 ${MODULE} 模块..."
make -C "${MODULE_DIR}" clean 2>/dev/null || true
make -C "${MODULE_DIR}" -j"$(nproc)"
echo "[BUILD] 编译完成: ${TARGET_HEX}"

# 第二步：烧录（可选）
if [ "${ACTION}" = "flash" ]; then
    echo ""
    echo "[FLASH] 准备通过 J-Link 烧录 ${TARGET_HEX} ..."

    # 检查 JLinkExe 是否可用
    if ! command -v JLinkExe &> /dev/null; then
        echo "[ERROR] 未找到 JLinkExe，请先安装 Segger J-Link 软件包"
        echo "        下载: https://www.segger.com/downloads/jlink/"
        exit 1
    fi

    # 检查 hex 文件是否存在
    if [ ! -f "${TARGET_HEX}" ]; then
        echo "[ERROR] 未找到 hex 文件: ${TARGET_HEX}"
        exit 1
    fi

    # 生成临时的 J-Link 命令脚本
    JLINK_SCRIPT="/tmp/jlink_flash_${MODULE}.jlink"
    cat > "${JLINK_SCRIPT}" << EOF
device S32K144
si SWD
speed 4000
autoconnect 1
loadfile ${TARGET_HEX}
r
g
q
EOF

    echo "[FLASH] 执行 J-Link 烧录..."
    JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1 \
             -CommanderScript "${JLINK_SCRIPT}"

    # 清理临时脚本
    rm -f "${JLINK_SCRIPT}"

    echo "[FLASH] 烧录完成!"
fi

echo ""
echo "[DONE] ${MODULE} 模块处理完毕"
