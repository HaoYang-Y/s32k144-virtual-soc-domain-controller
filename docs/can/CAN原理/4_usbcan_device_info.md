# USBCAN 设备信息

> 测试日期：2026-05-22 | 测试环境：Ubuntu 24.04 (VMware), Linux 6.17

## 设备规格

| 项目 | 值 |
|------|-----|
| 设备型号 | CANable（candleLight 固件） |
| 主控芯片 | STM32 |
| USB VID:PID | `1d50:606f` |
| 类型 | CANable（基于开源 candleLight 固件） |
| Linux 驱动 | `gs_usb`（内核自带，即插即用） |
| CAN 控制器时钟 | 48 MHz |
| 支持波特率 | 10k ~ 1Mbps |
| 终端电阻 | 可插拔 120Ω |

## 测试结果

- **波特率：500kbps** ✅
- 发送 3 帧（123#DEADBEEF, 456#01020304, 789#AABBCCDD）— 全部成功
- 接收 3 帧（loopback 回环共 6 帧）— 全部成功
- 错误统计：errors=0, dropped=0
- 最终状态：ERROR-ACTIVE
