#!/usr/bin/env bash
# ==========================================================================
# can_setup.sh — S32K144 CAN 通信测试工具
# ==========================================================================
# 用途:  配置 CANable/candleLight 为 SocketCAN can0，提供 candump/cansend 快捷操作
# 硬件:  CANable USB-CAN (gs_usb, 1d50:606f) 连接 Ubuntu VM → S32K144 FlexCAN0
# 用法:
#   ./can_setup.sh up          启动 can0 (500kbps)
#   ./can_setup.sh down        关闭 can0
#   ./can_setup.sh monitor     启动 candump 监控 (Ctrl+C 停止)
#   ./can_setup.sh send [id] [data]   发送 CAN 帧
#   ./can_setup.sh loopback    CANable 硬件 loopback 测试
# ==========================================================================

set -euo pipefail

CAN_IF="${CAN_IF:-can0}"
BITRATE="${BITRATE:-500000}"
CANABLE_VID="1d50"
CANABLE_PID="606f"

# ========================== 工具函数 ==========================

check_canable() {
    if ! lsusb -d "${CANABLE_VID}:${CANABLE_PID}" &>/dev/null; then
        echo "[ERR] CANable (${CANABLE_VID}:${CANABLE_PID}) 未检测到"
        echo "      请检查 USB 直通到 VM 是否正常"
        exit 1
    fi
    echo "[OK] CANable 设备已检测到"
}

check_gs_usb() {
    if ! lsmod | grep -q gs_usb; then
        echo "[INFO] 加载 gs_usb 内核模块..."
        sudo modprobe gs_usb
    fi
    echo "[OK] gs_usb 驱动已加载"
}

# ========================== 命令实现 ==========================

can_up() {
    check_canable
    check_gs_usb

    sudo ip link set "$CAN_IF" down 2>/dev/null || true
    sudo ip link set "$CAN_IF" type can bitrate "$BITRATE" restart-ms 100
    sudo ip link set "$CAN_IF" up

    echo "[OK] $CAN_IF 已启动 (${BITRATE} bps)"
    echo ""
    ip -details link show "$CAN_IF"
    echo ""
    echo "[TIP] 运行 './can_setup.sh monitor' 启动 candump 监控"
}

can_down() {
    sudo ip link set "$CAN_IF" down 2>/dev/null || true
    echo "[OK] $CAN_IF 已关闭"
}

monitor() {
    if ! ip link show "$CAN_IF" &>/dev/null; then
        echo "[ERR] $CAN_IF 接口不存在，请先运行 './can_setup.sh up'"
        exit 1
    fi
    echo "[INFO] 开始监控 $CAN_IF (Ctrl+C 停止)..."
    echo "      期望每 ~500ms 收到一帧: 123#XXXXXXXXAA55AA55"
    echo ""
    candump -td -c "$CAN_IF"
}

send_test() {
    local id="${1:-123}"
    local data="${2:-DEADBEEF01020304}"

    if ! ip link show "$CAN_IF" &>/dev/null; then
        echo "[ERR] $CAN_IF 接口不存在，请先运行 './can_setup.sh up'"
        exit 1
    fi

    echo "[SEND] can0 ${id}#${data} ..."
    cansend "$CAN_IF" "${id}#${data}"
    echo "[OK] 已发送"
}

loopback_test() {
    echo "======================================================"
    echo "  CANable 硬件 Loopback 测试"
    echo "======================================================"
    echo ""
    echo "  前提条件:"
    echo "    1. CANable 上 120Ω 终端电阻开关已打开"
    echo "    2. CAN_H 和 CAN_L 用跳线短接 (或接有终端电阻的正常总线)"
    echo ""

    can_up
    sleep 1

    echo ""
    echo "[TEST] 发送 3 帧测试帧..."
    echo ""

    local i
    for i in 1 2 3; do
        local data
        case $i in
            1) data="DEADBEEF01020304" ;;
            2) data="AABBCCDD55667788" ;;
            3) data="11223344ABCDEF00" ;;
        esac
        echo "[SEND] 123#${data}"
        cansend "$CAN_IF" "123#${data}"
        sleep 0.3
    done

    sleep 0.5
    echo ""
    echo "  预期结果: candump 显示 3 帧 sent + 3 帧 received (共 6 帧)"
    echo "  如果只看到 send 没有 receive: 检查终端电阻和接线"
    echo ""
    echo "[DONE] 测试完成"
}

# ========================== 入口 ==========================

case "${1:-}" in
    up)
        can_up
        ;;
    down)
        can_down
        ;;
    monitor)
        monitor
        ;;
    send)
        send_test "${2:-123}" "${3:-DEADBEEF01020304}"
        ;;
    loopback)
        loopback_test
        ;;
    *)
        echo "用法: $0 {up|down|monitor|send [id] [data]|loopback}"
        echo ""
        echo "  up       启动 can0 (${BITRATE} bps)"
        echo "  down     关闭 can0"
        echo "  monitor  启动 candump 监控"
        echo "  send     发送 CAN 帧 (默认: 123#DEADBEEF01020304)"
        echo "  loopback CANable 硬件回环测试"
        echo ""
        echo "  环境变量:"
        echo "    CAN_IF   网络接口名 (默认: can0)"
        echo "    BITRATE  波特率 (默认: 500000)"
        exit 1
        ;;
esac
