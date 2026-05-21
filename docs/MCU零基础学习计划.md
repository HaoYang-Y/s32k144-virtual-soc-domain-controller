# 🎯 学习计划 — CAN / SOME/IP / AUTOSAR 通信协议栈（配套 S32K14x 书籍）

> **目标人群**: 有 Linux C++ 基础，嵌入式 C 经验较少
> **配套书籍**: 《汽车电子S32K系列微控制器——基于ARM Cortex-M4F内核》（苏勇 著）
> **预计时长**: 8~12 周（每周 5~8 小时）
> **核心产出**: S32K144 通过 CAN 发送数据到 Linux 虚拟机，理解 AUTOSAR CP/AP 核心概念
> **原则**: 聚焦 CAN 主线，其他外设按需补充，不学寄存器级外设开发

---

## ⚡ 硬件拓扑

```
┌─────────────────┐     CAN H/L      ┌──────────────┐     USB 直通     ┌─────────────────┐
│  S32K144 开发板  ├─────────────────┤ USB-CAN 工具  ├─────────────────┤ Ubuntu 虚拟机   │
│                 │   120Ω 终端电阻   │ (CANable/     │                 │ (SocketCAN)     │
│  FlexCAN 模块    │                  │  USB2CAN)     │                 │ can0 / candump  │
└─────────────────┘                  └──────────────┘                 └─────────────────┘
```

> **说明**: 你的主力是 Linux 开发，MCU 寄存器驱动只是辅助手段，不深入外设细节。

---

## 学习路线总览

```
阶段 0：环境搭建与硬件拓扑                     ← 先让 CAN 能物理连通
        ↓
阶段 1：时钟 → FlexCAN 核心                   ← 聚焦 CAN 通信，其他外设略过
        ↓
阶段 2：SocketCAN + CAN 信号解析              ← SOC 端协议栈
        ↓
阶段 3：SOME/IP 服务通信                      ← 双主线之 SOME/IP
        ↓
阶段 4：UDS 诊断（后续扩展）
```

---

## 阶段 0：环境搭建与硬件拓扑（1 天）

### 硬件拓扑确认

```
S32K144 CAN0_TX (PTD1) ──→ CAN 收发器 ──→ CAN H ──→ USB-CAN 工具 ──→ USB ──→ Ubuntu 虚拟机
S32K144 CAN0_RX (PTD0) ──→ CAN 收发器 ──→ CAN L ──→ USB-CAN 工具 ──→ USB ──→ Ubuntu 虚拟机
```

- 确认开发板 CAN 收发器供电正常
- 确认 CAN 总线 H/L 两端接 **120Ω 终端电阻**
- 确认 USB-CAN 工具被 Ubuntu 虚拟机识别（USB 直通）

### 虚拟机侧：USB-CAN 驱动加载

USB-CAN 工具常见有两种芯片方案，需要加载对应内核模块：

| 工具方案 | Linux 驱动 | 加载命令 |
|---------|-----------|---------|
| CANable (slcan) | `slcan` | `sudo modprobe slcan` |
| USB2CAN (gs_usb) | `gs_usb` | `sudo modprobe gs_usb` |
| PCAN USB | `peak_usb` | `sudo modprobe peak_usb` |

```bash
# 插上 USB-CAN 工具后，查内核日志看识别到了什么
dmesg | tail -20

# 加载对应驱动
sudo modprobe gs_usb   # 以 gs_usb 为例，实际按你的工具选择

# 查看生成的 CAN 接口
ip link show

# 启动 CAN 接口
sudo ip link set can0 up type can bitrate 500000
sudo ip link set can0 up

# 验证：用 candump 监听
candump can0
```

### 开发环境确认

```bash
# 安装 ARM 交叉编译器
sudo apt install gcc-arm-none-eabi make

# 验证
arm-none-eabi-gcc --version
```

---

## 阶段 1：时钟 + FlexCAN 核心（2~4 周）

> 直接跳到 CAN 通信核心。其他外设（GPIO/UART/TIMER/ADC）用到时再学。

### Step 1：C 语言嵌入式必备知识点（配合书籍第 1~2 章）

这是 MCU 编程的基础，需要掌握才能看懂寄存器操作：

| 知识点 | 为什么重要 | 本项目应用 | 书籍对应 |
|--------|-----------|-----------|---------|
| **指针与地址操作** | 寄存器就是内存地址 | `*(volatile uint32_t*)addr = val;` | 1.3 节 |
| **位运算**（&、\|、~、<<、>>） | 配置寄存器特定位 | `reg |= (1 << 5);` | 2.4 节 |
| **volatile 关键字** | 防止编译器优化寄存器读取 | `volatile uint32_t` | 1.5 节 |
| **结构体指针映射** | 分组访问外设寄存器 | `CAN_Type *can = (CAN_Type*)base;` | 2.5 节 |
| **宏定义与枚举** | 寄存器地址/掩码抽象 | `#define MCR(x) *(volatile uint32_t*)((x) + 0x00)` | - |
| **链接脚本** (.ld) | 内存布局、段定义 | `s32k144_flash.ld` | 3.3 节 |

### Step 2：时钟系统（配合书籍第 3 章）

> FlexCAN 的时钟源来自系统时钟，必须先配好时钟。

| 书籍内容 | 项目对应代码 | 重点理解 |
|---------|------------|---------|
| 3.1 时钟系统概述 | `mcu/clock/README.md` | 时钟树结构 |
| 3.2-3.5 配置流程 | `mcu/clock/src/clock_driver.c` | SCGOUT/SPLL/分频器 |

✅ **完成标准**: 系统时钟配置到 80MHz（SPLL），CAN 外设时钟使能

### Step 3：FlexCAN 寄存器驱动（配合书籍第 10 章）

> **核心目标**: S32K144 能通过 CAN 总线发送 CAN 帧，Ubuntu 虚拟机用 `candump` 收到。

| 书籍内容 | 项目对应代码 | 重点理解 |
|---------|------------|---------|
| 10.1 CAN 协议基础 | `mcu/flexcan/README.md` | 帧格式、仲裁、位填充 |
| 10.2 FlexCAN 概述 | `mcu/flexcan/include/flexcan_driver.h` | CAN 协议引擎 |
| 10.3-10.4 寄存器描述 | `mcu/flexcan/src/flexcan_driver.c` | CTRL/MCR/IFLAG/MB |
| 10.5-10.6 驱动实现 | flexcan_driver.c 中的 TODO | 初始化、发送、接收、中断 |

**验证方法**：
```
MCU 端 (S32K144)               Linux 虚拟机
  ┌──────────────┐   CAN 帧    ┌──────────────┐
  │ flexcan_init │ ────────→   │ candump can0 │
  │ (500kbps)    │             │ 收到 0x123#01 │
  │ 定时发帧     │             │ 02 03 04     │
  └──────────────┘             └──────────────┘
```

✅ **完成标准**: `candump can0` 在虚拟机上能稳定收到 S32K144 发出的 CAN 帧（500kbps）

**🔄 AUTOSAR CP 概念穿插**：学完本节后阅读 `mcu/flexcan/README.md` 底部的 AUTOSAR 对照表，理解 MCAL Can 模块（Can_Init/Can_Write/Can_Read）与寄存器驱动的关系。

---

## 阶段 2：SocketCAN + CAN 信号解析（3~4 周）← 核心

> 从 MCU 端上升到 SOC 端（Linux），使用 SocketCAN 实现 CAN 通信和信号解析

### 2.1 SocketCAN SOC 端收发

| 学习内容 | 项目对应代码 | 重点理解 |
|---------|------------|---------|
| SocketCAN 基础 | `soc/include/can_manager.h` | AF_CAN / SOCK_RAW |
| CAN 帧收/发 | `soc/src/can_manager/can_manager.cpp` | send / recv / select |
| 接收线程/回调 | can_manager.cpp 中的 TODO | 线程安全、环形缓冲区 |

**🔄 AUTOSAR CP 概念穿插**：理解 CanIf（CAN 接口层）的角色——它位于 MCAL Can 上层，抽象具体 CAN 硬件，为上层协议栈统一收发接口。

✅ **完成标准**: SOC 端程序能通过 SocketCAN 收发 CAN 帧

### 2.2 CAN 信号解析（DBC 概念）

| 学习内容 | 项目对应代码 | 重点理解 |
|---------|------------|---------|
| CAN 信号定义 | `config/domain_config.yaml` signals 段 | DBC 基本元素 |
| 信号提取算法 | `soc/include/signal_manager.h` | Intel/Motorola 字节序 |
| 信号融合框架 | `soc/src/signal_fusion/signal_manager.cpp` | 信号缓存、变化通知 |

**🔄 AUTOSAR CP 概念穿插**：理解 PduR（PDU 路由器）和 Com（通信管理器）的角色。

✅ **完成标准**: SOC 端程序能解析 CAN 帧中的信号，输出物理值到控制台

### 2.3 CAN 网络管理（CanNm 概念了解）

> 了解 AUTOSAR CP CanNm 网络管理机制——基于 CAN 的节点睡眠/唤醒协调

此环节为概念学习，不要求实现。

✅ **完成标准**: 能在思维导图中画出 AUTOSAR CP CAN 协议栈分层（Can → CanIf → PduR → Com → CanNm）

---

## 阶段 3：SOME/IP 服务通信（3~4 周）← 双主线之 SOME/IP

> 从 CAN 的"信号级"通信上升到 IP 网络的"服务级"通信，每层附 AUTOSAR AP 概念

### 3.1 SOME/IP 协议基础

| 学习内容 | 项目相关代码/配置 | 重点理解 |
|---------|-----------------|---------|
| SOME/IP 报文头 | `soc/src/someip_service/`（待实现） | Message ID / Length / Session ID |
| Request/Response | someip_service/ 中的 TODO 骨架 | 服务发现基础 |
| Fire & Forget | 配置 `config/domain_config.yaml` | 事件通知机制 |

**🔄 AUTOSAR AP 概念穿插**：理解 `ara::com` 服务模型（Method / Event / Field）

✅ **完成标准**: 理解 SOME/IP 报文结构，能在 wireshark 中解析 SOME/IP 报文

### 3.2 SOME/IP 服务发现（SD）

**🔄 AUTOSAR AP 概念穿插**：`ara::sd` 服务发现

✅ **完成标准**: 理解 SOME/IP SD 的 Offer/Subscribe 流程

### 3.3 SOME/IP 序列化

**🔄 AUTOSAR AP 概念穿插**：理解 E2E（端到端保护）

✅ **完成标准**: 能手动计算一个简单 SOME/IP 报文的序列化字节

---

## 阶段 4：UDS 诊断（ISO 14229）— 后续扩展

| 书籍参考 | 关键服务 | 项目预留位置 |
|---------|---------|-------------|
| 后续 UDS 章节 | 0x10 会话控制 | `soc/src/uds_server/` |
| (可参考其他资料) | 0x22 读 DID | `config/domain_config.yaml` uds 段 |
| | 0x2E 写 DID | (待实现) |
| | 0x19 读取 DTC | (待实现) |

---

## 书籍配套指南：推荐阅读顺序

```
第 1~2 章   C 基础概览 (1 天)           ← 必须看
    ↓
第 3 章     时钟系统 (2 天)             ← 必须看，CAN 依赖
    ↓
第 10 章    FlexCAN (1~2 周)           ← ★ 核心
    ↓
可选（用到时再看）：
  - 第 4 章  GPIO     ← 调试 LED 指示时
  - 第 5 章  UART     ← 需串口打印调试时
  - 第 7 章  PIT      ← 需精确定时时
  - 第 8 章  ADC      ← 需采集模拟信号时
  - 第 9 章  FTM      ← 需 PWM 时
```

---

## 总结

| 阶段 | 内容 | 时间 | 产出 |
|------|------|------|------|
| 0 | 环境搭建 + 硬件拓扑 | 1 天 | USB-CAN 驱动成功，candump 能监听 |
| 1 | 时钟 + FlexCAN | 2~4 周 | S32K144 发 CAN 帧到虚拟机 |
| 2 | SocketCAN + CAN 信号解析 | 3~4 周 | SOC 端 CAN 协议栈 |
| 3 | SOME/IP 服务通信 | 3~4 周 | 服务发现 + 序列化 |
| 4 | UDS 诊断 | 后续 | 诊断协议栈 |

> **一句话**: 不需要深入 MCU 寄存器开发，但需要理解 CAN 通信链路——从 S32K144 发 CAN 帧，到虚拟机 SocketCAN 收到并解析信号。
