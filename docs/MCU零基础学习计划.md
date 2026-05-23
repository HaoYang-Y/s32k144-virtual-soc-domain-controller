# 🎯 学习计划 — CAN / SOME/IP / AUTOSAR 通信协议栈（基于 NXP S32 SDK）

> **目标人群**: 有 Linux C++ 基础，嵌入式 C 经验较少
> **预计时长**: 8~12 周（每周 5~8 小时）
> **核心产出**: S32K144 接收 CAN 帧 → 解析信号 → UART 传给 SOC → SOC 封装 SOME/IP 发送
> **原则**: 聚焦 CAN 主线，先独立学各外设，再组合实现域控制器

---

## 🧩 SDK 说明

本工程 MCU 代码基于 **NXP S32 SDK for S32K1xx (RTM 4.0.2)** 开发，SDK 位于
`mcu/S32_SDK_S32K1xx_RTM_4.0.2/`。

### SDK 说明：使用 SDK API 替代寄存器操作

本工程使用 NXP S32 SDK 提供的 HAL/PD 层 API，不涉及直接寄存器操作：

| 传统方式（本书不采用） | SDK 方式（本工程采用） |
|---------|---------|
| 手写 `*(volatile uint32_t*)addr` 操作寄存器 | SDK API：`FLEXCAN_DRV_Init()`、`ADC_DRV_Init()` 等 |
| 逐寄存器配置外设 | SDK 驱动层：`flexcan_driver.h`、`lpuart_driver.h` |
| 手写 startup 汇编 | SDK 提供 `startup_S32K144.S` |
| 手写时钟配置 | SDK 提供 `clock_manager.h` + `CLOCK_SYS_*` API |

### 学习策略

> **"理解原理，使用 SDK"**——先了解外设的基本工作原理，然后直接使用 SDK API 进行开发。
> 每个外设的学习分为两步：
> 1. 📖 阅读 SDK 头文件，理解外设功能和使用方式
> 2. 🛠 调用 SDK API 实现功能

例如 FlexCAN 模块：
- 📖 查看 `flexcan_driver.h` 中的 API 和配置结构体，理解初始化/发送/接收流程
- 🛠 实际代码中调用 `FLEXCAN_DRV_Init()`、`FLEXCAN_DRV_SendBlocking()` 等 API

### SDK 头文件路径速查

| 外设 | SDK 头文件（相对 SDK 根目录） | 常用 API 前缀 |
|------|------------------------------|--------------|
| FlexCAN | `platform/drivers/inc/flexcan_driver.h` | `FLEXCAN_DRV_*` |
| UART (LPUART) | `platform/drivers/inc/lpuart_driver.h` | `LPUART_DRV_*` |
| GPIO/Pins | `platform/drivers/inc/pins_driver.h` | `PINS_DRV_*` |
| Timer (LPIT) | `platform/drivers/inc/lpit_driver.h` | `LPIT_DRV_*` |
| Timer (LPTMR) | `platform/drivers/inc/lptmr_driver.h` | `LPTMR_DRV_*` |
| ADC | `platform/drivers/inc/adc_driver.h` | `ADC_DRV_*` |
| Clock | `platform/drivers/inc/clock_manager.h` | `CLOCK_SYS_*` |
| 中断管理 | `platform/drivers/inc/interrupt_manager.h` | `INT_SYS_*` |
| 设备寄存器定义 | `platform/devices/S32K144.h` | `CAN0_BASE`、`PCC_CAN0` 等宏 |

---

## ⚡ 硬件拓扑

```
┌─────────────────────────────────────────────────────────────────────┐
│                       域控制器完整数据流                             │
│                                                                     │
│  CAN 总线 ──── CAN 帧 (500kbps) ────► S32K144 MCU                  │
│                                          │                          │
│                                          ├─ FlexCAN 接收 CAN 帧     │
│                                          ├─ DBC 信号解析            │
│                                          └─ UART 结构化信号输出     │
│                                              │                      │
│                                              │ USB-UART (115200bps) │
│                                              ▼                      │
│                                        Ubuntu 虚拟机 (SOC)          │
│                                          │                          │
│                                          ├─ UART 接收/解析          │
│                                          ├─ 信号融合                │
│                                          ├─ SOME/IP 序列化          │
│                                          └─ 车载以太网 ──► 其他域    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

开发阶段拓扑（独立模块验证时）：
┌─────────────────┐     CAN H/L      ┌──────────────┐     USB 直通     ┌─────────────────┐
│  S32K144 开发板  ├─────────────────┤ USB-CAN 工具  ├─────────────────┤ Ubuntu 虚拟机   │
│                 │   120Ω 终端电阻   │ (CANable/     │                 │ (SocketCAN)     │
│  FlexCAN 模块    │                  │  USB2CAN)     │                 │ can0 / candump  │
└────────┬────────┘                  └──────────────┘                 └─────────────────┘
         │
         │ USB-UART (仅 domain_controller 联调时用)
         ▼
┌─────────────────┐
│  Ubuntu 虚拟机   │
│  (ttyUSB0)       │
└─────────────────┘
```

> **说明**: 独立模块学习阶段，通过 USB-CAN 工具验证 FlexCAN 收发。进入域控制器联调阶段后，
> 改为 CAN 总线输入给 MCU，MCU 解析后通过 UART 输出给 SOC，USB-CAN 工具不再直接连 SOC。

---

## 学习路线总览

```
阶段 0：环境搭建与硬件拓扑                     ← 先让 CAN 能物理连通
        ↓
阶段 1：时钟 → FlexCAN 核心                   ← 聚焦 CAN 通信，其他外设略过
        ↓
阶段 2：MCU 端域控制器应用 (domain_controller) ← CAN 收 → 解析 → UART 发
        ↓
阶段 3：SOC 端 UART 接收 + 信号融合            ← 结合阶段 2 联调
        ↓
阶段 4：SOME/IP 服务通信                      ← 双主线之 SOME/IP
        ↓
阶段 5：UDS 诊断（后续扩展）
```

---

## 阶段 0：环境搭建与硬件拓扑（1 天）

### 硬件拓扑确认（独立模块验证阶段）

```
S32K144 CAN0_TX (PTD1) ──→ CAN 收发器 ──→ CAN H ──→ USB-CAN 工具 ──→ USB ──→ Ubuntu 虚拟机
S32K144 CAN0_RX (PTD0) ──→ CAN 收发器 ──→ CAN L ──→ USB-CAN 工具 ──→ USB ──→ Ubuntu 虚拟机
```

> **提示**: 阶段 2 联调时，USB-CAN 工具只连接 MCU 的 CAN 总线（产生 CAN 帧输入给 MCU），
> MCU 通过独立的 USB-UART 线连接到 SOC 虚拟机传输解析后的信号。

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

### Step 1：C 语言嵌入式必备知识点

这是 MCU 编程的基础，理解下面这些知识点有助于阅读 SDK 头文件和本项目代码：

| 知识点 | 为什么重要 | 本项目应用 |
|--------|-----------|-----------|
| **typedef / 结构体** | SDK 用结构体封装配置参数 | `flexcan_user_config_t`、`lpit_user_config_t` |
| **枚举与宏定义** | SDK 使用枚举定义模式/状态 | `can_frame_type_t`、`clock_source_t` |
| **volatile 关键字** | 防止编译器优化外设寄存器读取 | SDK 内部使用 |
| **函数指针** | SDK 回调机制 | 中断回调注册 |
| **链接脚本** (.ld) | 内存布局、段定义 | `s32k144_flash.ld` |

### Step 2：时钟系统

> FlexCAN 的时钟源来自系统时钟，必须先配好时钟。

| 项目对应代码 | 重点理解 |
|------------|---------|
| `mcu/src/clock.c` | 时钟源初始化流程 |
| `mcu/include/clock.h` | 时钟源枚举、API 声明 |

**SDK API 调用流程**：
```c
// 1. 加载配置
CLOCK_SYS_Init(NULL);
// 2. 应用配置
CLOCK_SYS_SetConfiguration(NULL);
// 3. 切换系统时钟源
CLOCK_SYS_SetSource(CLOCK_SRC_SPLL);
```

✅ **完成标准**: 理解时钟树结构（SOSC→SPLL→CORE_CLK），能够调用 SDK API 配置系统时钟

### Step 3：FlexCAN 驱动

> **核心目标**: S32K144 能通过 CAN 总线发送 CAN 帧，Ubuntu 虚拟机用 `candump` 收到。

| 项目对应代码 | 重点理解 |
|------------|---------|
| `mcu/include/flexcan.h` | CAN 帧结构体定义、API 声明 |
| `mcu/src/flexcan.c` | SDK API：`FLEXCAN_DRV_Init()`、`FLEXCAN_DRV_SendBlocking()`、`FLEXCAN_DRV_ReceiveBlocking()` |
| `mcu/Makefile` | SDK 头文件路径、库链接 |

**验证方法**（独立模块测试）：
```
MCU 端 (S32K144)               Linux 虚拟机
  ┌──────────────┐   CAN 帧    ┌──────────────┐
  │ flexcan_init │ ────────→   │ candump can0 │
  │ (500kbps)    │             │ 收到 0x123#01 │
  │ 定时发帧     │             │ 02 03 04     │
  └──────────────┘             └──────────────┘
```

✅ **完成标准**: `candump can0` 在虚拟机上能稳定收到 S32K144 发出的 CAN 帧（500kbps）

**🔄 AUTOSAR CP 概念穿插**：了解 MCAL Can 模块（Can_Init/Can_Write/Can_Read）与 SDK 驱动 API 的对应关系。

---

## 阶段 2：MCU 端 domain_controller 应用（2~3 周）

> **核心阶段**: 将 FlexCAN + UART 组合为域控制器网关应用。
> 通过 SDK API 实现 MCU 接收 CAN 帧 → 解析信号 → 通过 UART 发送结构化数据给 SOC。

### 2.1 应用代码

| 文件 | 职责 | 依赖的 SDK API |
|------|------|--------------|
| `mcu/src/flexcan.c` | FlexCAN 接收 CAN 帧 | `FLEXCAN_DRV_Init()`、`FLEXCAN_DRV_ReceiveBlocking()` |
| `mcu/src/uart.c` | UART 发送结构化数据 | `UART_DRV_Init()`、`UART_DRV_SendDataBlocking()` |
| `mcu/src/clock.c` | 系统时钟配置 | `CLOCK_SYS_Init()`、`CLOCK_SYS_SetConfiguration()` |

### 2.2 UART 自定义传输协议

```
帧结构 (8 字节)：
┌────────┬────────┬────────────┬────────────┬────────┐
│ 0xAA   │ 0x55   │ Signal ID  │ Value      │ CRC8   │
│ (帧头)  │ (帧头)  │ (2 字节)   │ (4 字节)    │ (1 字节)│
└────────┴────────┴────────────┴────────────┴────────┘
```

| 字段 | 大小 | 说明 |
|------|------|------|
| 帧头 0xAA 0x55 | 2 字节 | 帧同步，检测帧起始 |
| Signal ID | 2 字节 | 信号标识符（小端，如 0x0001 = 车速） |
| Value | 4 字节 | IEEE 754 float 或 int32 物理值 |
| CRC8 | 1 字节 | 前 8 字节的 CRC8 校验，使用 0x07 多项式 |

**波特率**: 115200 bps，8N1

### 2.3 信号解析示例（DBC 映射）

| CAN ID | 信号名 | 起始位 | 长度 | 因子 | 偏移 | 物理范围 |
|--------|--------|--------|------|------|------|---------|
| 0x123 | 车速 | 0 | 16 | 0.01 | 0 | 0~655.35 km/h |
| 0x123 | 发动机转速 | 16 | 16 | 0.125 | 0 | 0~8191.875 rpm |
| 0x456 | 油门开度 | 0 | 8 | 0.4 | 0 | 0~102 % |

**验证**：MCU 收到 CAN 帧 → 解析信号 → UART 发出 → PC 串口助手或 minicom 查看

✅ **完成标准**: MCU 能接收 CAN 帧，解析出物理信号值，通过 UART 输出到 PC 串口显示

---

## 阶段 3：SOC 端 UART 接收 + 信号融合（3~4 周）

> 从 MCU 端上升到 SOC 端（Linux），SOC 通过 USB-UART 接收 MCU 发来的结构化信号数据。

### 3.1 UART 接收管理器

| 学习内容 | 项目对应代码 | 重点理解 |
|---------|------------|---------|
| Linux 串口编程 | `soc/include/can_manager.h` | termios 配置、非阻塞读 |
| 自定义协议解析 | `soc/src/can_manager/can_manager.cpp` | 帧同步、CRC8 校验、环形缓冲区 |
| 信号管理 | `soc/src/signal_fusion/signal_manager.cpp` | 字节序转换、物理值缓存 |

**🔄 AUTOSAR 概念穿插**：理解 PduR（PDU 路由器）和 Com（通信管理器）的角色——UART 收到的结构化信号对应 AUTOSAR 中的 I-PDU。

✅ **完成标准**: SOC 端程序能通过 USB-UART 接收 MCU 发来的信号数据，输出到控制台

### 3.2 SOC ↔ MCU 联调

```
MCU 端 (S32K144) ──CAN帧──► USB-CAN 工具 (CAN 总线)
             │
             │ CAN 帧
             ▼
       MCU domain_controller  ← 从 CAN 总线收帧
             │
             │ 解析出信号 → 组帧 UART
             ▼
       USB-UART 线
             │
             ▼
       SOC (Ubuntu) ── 接收/解析信号 ── 控制台输出
```

✅ **完成标准**: MCU 端通过 USB-CAN 工具接收 CAN 帧，解析后通过 UART 传给 SOC，SOC 控制台正确显示信号值

### 3.3 CAN 网络管理（CanNm 概念了解）

> 了解 AUTOSAR CP CanNm 网络管理机制——基于 CAN 的节点睡眠/唤醒协调

此环节为概念学习，不要求实现。

✅ **完成标准**: 能在思维导图中画出 AUTOSAR CP CAN 协议栈分层（Can → CanIf → PduR → Com → CanNm）

---

## 阶段 4：SOME/IP 服务通信（3~4 周）← 双主线之 SOME/IP

> 从 CAN 的"信号级"通信上升到 IP 网络的"服务级"通信

### 4.1 SOME/IP 协议基础

| 学习内容 | 项目相关代码/配置 | 重点理解 |
|---------|-----------------|---------|
| SOME/IP 报文头 | `soc/src/someip_service/`（待实现） | Message ID / Length / Session ID |
| Request/Response | someip_service/ 中的 TODO 骨架 | 服务发现基础 |
| Fire & Forget | 配置 `config/domain_config.yaml` | 事件通知机制 |

**🔄 AUTOSAR AP 概念穿插**：理解 `ara::com` 服务模型（Method / Event / Field）

✅ **完成标准**: 理解 SOME/IP 报文结构，能在 wireshark 中解析 SOME/IP 报文

### 4.2 SOME/IP 服务发现（SD）

**🔄 AUTOSAR AP 概念穿插**：`ara::sd` 服务发现

✅ **完成标准**: 理解 SOME/IP SD 的 Offer/Subscribe 流程

### 4.3 SOME/IP 序列化

**🔄 AUTOSAR AP 概念穿插**：理解 E2E（端到端保护）

✅ **完成标准**: 能手动计算一个简单 SOME/IP 报文的序列化字节

### 4.4 全链路联调

```
CAN 总线 ──► MCU (FlexCAN 收帧) ──► 信号解析 ──► UART ──► SOC
                                                              │
                                                     signal_manager
                                                              │
                                                     someip_server
                                                              │
                                                     ──► 其他域 (以太网)
```

✅ **完成标准**: CAN 帧从总线输入，最终作为 SOME/IP 服务发布

---

## 阶段 5：UDS 诊断（ISO 14229）— 后续扩展

| 关键服务 | 项目预留位置 |
|---------|-------------|
| 0x10 会话控制 | `soc/src/uds_server/` |
| 0x22 读 DID | `config/domain_config.yaml` uds 段 |
| 0x2E 写 DID | (待实现) |
| 0x19 读取 DTC | (待实现) |

---

## 总结

| 阶段 | 内容 | 时间 | 产出 |
|------|------|------|------|
| 0 | 环境搭建 + 硬件拓扑 | 1 天 | USB-CAN 驱动成功，candump 能监听 |
| 1 | 时钟 + FlexCAN | 2~4 周 | S32K144 收发 CAN 帧 |
| 2 | MCU 端 domain_controller | 2~3 周 | CAN 收帧 → 解析 → UART 发 |
| 3 | SOC 端 UART + 信号融合 | 3~4 周 | MCU ↔ SOC UART 全链路联调 |
| 4 | SOME/IP 服务通信 | 3~4 周 | 服务发现 + 序列化 + 全链路 |
| 5 | UDS 诊断 | 后续 | 诊断协议栈 |

> **一句话**: 从 MCU SDK API 到 SOC 服务——先独立学各外设，再组合实现
> "CAN→MCU 解析→UART→SOC→SOME/IP" 完整域控制器链路。
