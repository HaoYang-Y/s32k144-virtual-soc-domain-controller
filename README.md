# s32k144-virtual-soc-domain-controller — 基于 S32K144 与虚拟 SOC 的域控制器学习工程

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/MCU-S32K144-blue)](.)
[![Build](https://img.shields.io/badge/Build-Makefile%20%7C%20CMake-green)](.)
[![GitHub Stars](https://img.shields.io/github/stars/HaoYang-Y/s32k144-virtual-soc-domain-controller?style=social)](https://github.com/HaoYang-Y/s32k144-virtual-soc-domain-controller)

> **仓库地址**
>
> [![GitHub](https://img.shields.io/badge/GitHub-HaoYang--Y/s32k144--virtual--soc--domain--controller-181717?logo=github)](https://github.com/HaoYang-Y/s32k144-virtual-soc-domain-controller)
> [![Gitee](https://img.shields.io/badge/Gitee-Fighter--CTN/s32k144--virtual--soc--domain--controller-C71D23?logo=gitee)](https://gitee.com/Fighter-CTN/s32k144-virtual-soc-domain-controller)

> **车载信号网关原型 — S32K144 MCU 采集物理信号 → SPI → Ubuntu SOC → SOME/IP + CAN 发布**
>
> S32K144 开发板采集 GPIO 按键，通过 SPI/CAN/UART 与 Ubuntu 虚拟机通信，
> SOC 端差分解码后通过 SOME/IP 服务和 CAN 帧发布到其他域控制器。
> 代码按 AUTOSAR CP 六层（MCAL/ECU Abstraction/CDD/Services/RTE/SWC）和 AP 三层（Platform/Communication/App）组织，
> 模拟真实车载域控制器架构。

---

## 📋 目录

- [当前任务（优先级排列）](#-当前任务优先级排列)
- [项目架构](#-项目架构)
- [学习路线](#-学习路线)
- [项目结构](#-项目结构)
- [快速开始](#-快速开始)
- [文档索引](#-文档索引)
- [硬件要求](#-硬件要求)
- [License](#-license)

---

## 🎯 当前任务（优先级排列）

> 以下任务按优先级从高到低排列。标 ✅ 的已完成，标 ⏳ 的有骨架代码待调通，标 ❌ 的尚未开始。

### P0 — 阻塞性基础设施（正在攻坚）

| # | 任务 | 涉及模块 | 关键文件 | 状态 |
|---|------|---------|---------|------|
| 1 | **CAN 收发调通** | MCAL/Can, EcuAbstraction/CanIf | `mcu/MCAL/Can/src/Can.c`, `mcu/EcuAbstraction/CanIf/src/CanIf.c` | ✅ 已通 (含 TX 确认链) |
| 2 | **UART 日志输出** | CDD/Uart | `mcu/CDD/Uart/src/Uart.c` | ❌ 未通 |

> CAN 是域控制器的核心通信手段，UART 是调试的基础。CAN 已全链路调通（TX 发送 + RX 接收含 FC 回复 + TX 确认链）；UART 代码已实现（Log 在用），但硬件 USB-UART 链路未验证。

### P1 — MCAL 外设驱动（部分已验证）

| # | 任务 | 涉及模块 | 关键文件 | 状态 |
|---|------|---------|---------|------|
| 3 | **GPIO 按键输入** | MCAL/Gpio, MCAL/Port | `mcu/MCAL/Gpio/src/Gpio.c`, `mcu/MCAL/Port/src/Port.c` | ✅ AUTOSAR 对齐 |
| 4 | **MCU 时钟配置** | MCAL/Mcu | `mcu/MCAL/Mcu/src/Mcu.c`, `mcu/MCAL/Mcu/src/clock_config.c` | ✅ AUTOSAR 对齐 |
| 5 | **SPI 驱动** | MCAL/Spi | `mcu/MCAL/Spi/src/Spi.c` | ⏳ 骨架已有 |
| 6 | **IoHwAb 硬件抽象** | EcuAbstraction/IoHwAb | `mcu/EcuAbstraction/IoHwAb/src/IoHwAb.c` | ⏳ 骨架已有 |

### P2 — ECU 抽象层 + Services + RTE（骨架已有，部分已验证）

| # | 任务 | 涉及模块 | 关键文件 | 状态 |
|---|------|---------|---------|------|
| 7 | **CanIf 接口封装** | EcuAbstraction/CanIf | `mcu/EcuAbstraction/CanIf/src/CanIf.c` | ✅ 分层回调 (RX/TX) + hth 配置表 |
| 8 | **CanTp 传输层** | Services/CanTp | `mcu/Services/CanTp/src/CanTp.c` | ✅ SF/FF/MF + FC 流控 + TX 确认链 + N_As |
| 9 | **PduR 路由层** | Services/PduR | `mcu/Services/PduR/src/PduR.c` | ✅ 直通路由 + 确认路由 (路由表待 Com) |
| 10 | **EcuM 状态管理** | Services/EcuM | `mcu/Services/EcuM/src/EcuM.c` | ✅ Init + MainFunction 调度 (RX/TX/TP) |
| 11 | **SpiIf 接口封装** | EcuAbstraction/SpiIf | `mcu/EcuAbstraction/SpiIf/src/SpiIf.c` | ⏳ 骨架已有 |
| 12 | **RTE 运行时环境** | RTE | `mcu/RTE/Rte.c` | ⏳ 骨架已有 |

### P3 — SWC 信号采集 + 全链路（依赖 P2）

| # | 任务 | 涉及模块 | 关键文件 | 状态 |
|---|------|---------|---------|------|
| 13 | **SWC 信号采集调度** | App/Swc_SignalGateway | `mcu/App/Swc_SignalGateway/src/Swc_SignalGateway.c` | ⏳ 骨架已有 |
| 14 | **MCU↔SOC SPI 通信** | MCAL/Spi + soc/platform/Spi + soc/communication/SpiGateway | 双方 SPI 模块 | ❌ 未实现 |

### P4 — SOC 端服务（依赖 P3）

| # | 任务 | 涉及模块 | 关键文件 | 状态 |
|---|------|---------|---------|------|
| 15 | **SOC 信号融合** | soc/apps/DomainController | `soc/apps/DomainController/src/SignalFusion.cpp` | ⏳ 骨架已有 |
| 16 | **SOME/IP 服务发布** | soc/communication/ara/com | `soc/communication/ara/com/src/ServiceDiscovery.cpp` | ❌ 未实现 |
| 17 | **UDS 诊断服务** | soc/diag/ara/diag | `soc/diag/ara/diag/src/UdsServer.cpp` | ❌ 未实现 |

### P5 — 远期规划

| # | 任务 | 涉及模块 | 状态 |
|---|------|---------|------|
| 18 | **Protobuf 代码生成集成** | `proto/signal_gateway.proto` → MCU(nanopb) + SOC(protoc) | ❌ 未实现 |
| 19 | **DBC 信号矩阵** | CAN 信号编解码标准化 | ❌ 未实现 |

---

## 🏗 项目架构

### 物理拓扑

```
PC (宿主机)
├── Ubuntu 24.04 VM（SOC / MPU）
│   ├── Signal Bridge Daemon（vsomeip + libmpsse）
│   └── FT2232H（USB 直通，SPI Slave）
│
└── S32K144 开发板（MCU）
    ├── LPSPI0 ──── SPI Master → FT2232H
    ├── FlexCAN0 ── CAN → USB-CAN 分析仪
    └── GPIO 按键（车门×4 / 档位 P/R/N/D）
```

### 数据流

```
  GPIO 按键 ──中断──→ MCU 共享缓冲区（volatile struct）
                     │
              SWC 周期调度
                     │
          ┌──────────┼──────────┐
          ▼                     ▼
    SPI 32B 帧              CAN 帧
    (CMD+PAYLOAD+CRC8)     (FlexCAN0)
          │                     │
          ▼                     ▼
    FT2232H (USB)         USB-CAN 分析仪
          │                     │
          ▼                     ▼
    Ubuntu SOC (vsomeip)  candump / 其他 ECU
```

> 💡 **与实际工程的区别**
>
> 真实域控制器中，MCU 与 SOC 通过 **板载 SPI** 高速通信。本工程使用
> **FT2232H USB-SPI 桥**，通过 USB 直通到虚拟机，便于开发和调试。

### 软件分层

```
  MCU 端（裸机 C，AUTOSAR CP）                 SOC 端（C++，AUTOSAR AP）
  ┌──────────────────────────────────┐       ┌──────────────────────────────────┐
  │ App/Swc_SignalGateway (SWC)      │       │ apps/DomainController            │
  │   周期调度，读写 RTE 信号          │       │   ConfigManager + SignalFusion   │
  ├──────────────────────────────────┤       ├──────────────────────────────────┤
  │ RTE/                             │       │ communication/                   │
  │   Rte_Read/Rte_Write 零开销宏     │       │   ara/com (SOME/IP Service)      │
  ├──────────────────────────────────┤       │   SpiGateway (SPI 帧处理)         │
  │ Services/                        │  SPI  ├──────────────────────────────────┤
  │   BswM, Com, PduR, CanTp, EcuM   │◄─────►│ platform/                        │
  ├──────────────────────────────────┤ 32B帧 │   ara/log, ara/exec, ara/core    │
  │ EcuAbstraction/                  │       │   Spi/SpiDriver                  │
  │   CanIf / IoHwAb / SpiIf         │       ├──────────────────────────────────┤
  ├──────────────────────────────────┤       │ diag/ara/diag (UDS Server)       │
  │ CDD/Uart/                        │       └──────────────────────────────────┘
  ├──────────────────────────────────┤
  │ MCAL/ (NXP SDK API)              │
  │   Gpio / Mcu / Can / Spi / Port  │
  └──────────────────────────────────┘
```

---

## 📚 学习路线

> 详细学习计划见 [docs/MCU零基础学习计划.md](docs/MCU零基础学习计划.md)，架构设计见 [docs/VehicleGateway_Design.md](docs/VehicleGateway_Design.md)。

### 推荐学习顺序

```
第一阶段：MCAL 外设驱动（Gpio→Mcu→Can→Spi→Port）
         ↓        附 AUTOSAR CP MCAL 概念（标准化接口 Xxx_Init / Xxx_Read / Xxx_Write）
第二阶段：ECU 抽象层（CanIf→IoHwAb→SpiIf）
         ↓        把 MCAL 的硬件操作抽象为业务层可用的接口
第三阶段：RTE + SWC 应用层（Rte.h + Swc_SignalGateway）
         ↓        零开销宏连接 SWC 和底层驱动
第四阶段：CAN 通信栈（FlexCAN 收发 + CanTp 传输 + DBC 编解码）
         ↓        附 CP Can/CanIf/CanTp/PduR/Com 概念
第五阶段：SPI 通信（32B 固定帧 + 差分协议 + CRC8）
         ↓        附 CP Spi/Com Stack 概念
第六阶段：SOME/IP 服务通信（vsomeip + SD + Event）
         ↓        附 AP ara::com/ara::sd 概念
第七阶段：UDS 诊断（ISO 14229）
```

---

## 📁 项目结构

```
s32k144-virtual-soc-domain-controller/
│
├── README.md                    ← 项目说明
├── LICENSE                      ← Apache 2.0 开源协议
├── CMakeLists.txt               ← 顶层 CMake（SOC 和 MCU 独立构建）
│
├── docs/                        ← 📖 文档（按书本章节组织）
│   ├── VehicleGateway_Design.md    ← 架构设计（真相来源）
│   ├── MCU零基础学习计划.md         ← ★ 入门首选：从零学习路线
│   ├── AUTOSAR_学习路线图.md        ← AUTOSAR CP/AP 概念对照
│   ├── 从零学CAN.md                ← CAN 协议 + FlexCAN SDK 教程
│   ├── S32K144_DRV_层开发指南.md    ← NXP SDK DRV 层 API 参考
│   ├── MCU_交叉编译与烧录指南.md    ← 工具链 + J-Link 烧录
│   ├── TaskPlan.md                ← 任务跟踪
│   └── usbcan_device_info.md      ← USB-CAN 设备信息
│
├── mcu/                         ← ★ MCU 端（AUTOSAR CP，裸机 C）
│   ├── Makefile                 ← 构建脚本
│   ├── include/                 ← 公共头文件（Compiler.h, Std_Types.h, Platform_Types.h）
│   │
│   ├── MCAL/                    ← ① 微控制器抽象层（操作硬件寄存器）
│   │   ├── Gpio/    include/ src/ config/   GPIO 引脚读写
│   │   ├── Mcu/     include/ src/ config/   时钟配置
│   │   ├── Can/     include/ src/ config/   FlexCAN 收发
│   │   ├── Spi/     include/ src/ config/   LPSPI 收发
│   │   └── Port/    include/ src/ config/   引脚复用配置
│   │
│   ├── EcuAbstraction/           ← ② ECU 抽象层（硬件无关接口）
│   │   ├── CanIf/   include/ src/ config/   CAN 接口抽象
│   │   ├── IoHwAb/  include/ src/ config/   IO 硬件抽象
│   │   └── SpiIf/   include/ src/ config/   SPI 接口抽象
│   │
│   ├── CDD/                      ← ③ 复杂设备驱动层
│   │   └── Uart/    include/ src/ config/   UART 日志输出
│   │
│   ├── Services/                  ← ④ AUTOSAR CP 系统服务
│   │   ├── BswM/    include/ src/           模式管理
│   │   ├── Com/     include/ src/ config/   信号→PDU 编解码
│   │   ├── PduR/    include/ src/ config/   PDU 路由
│   │   ├── CanTp/   include/ src/ config/   CAN 传输层
│   │   ├── EcuM/    include/ src/           ECU 状态管理
│   │   └── Log/     include/ src/ config/   日志服务
│   │
│   ├── RTE/                      ← ⑤ 运行时环境（SWC↔BSW 桥梁）
│   │   ├── Rte.h                         RTE API 宏定义
│   │   ├── Rte.c                         缓冲区实现
│   │   └── Rte_Type.h                    共享信号类型定义
│   │
│   ├── App/                      ← ⑥ 应用层 SWC
│   │   └── Swc_SignalGateway/
│   │       include/ src/ config/          信号网关 SWC
│   │
│   └── S32_SDK_S32K1xx_RTM_4.0.2/  ← NXP S32 SDK（第三方）
│
├── soc/                         ← ★ SOC 端（AUTOSAR AP，C++17）
│   ├── CMakeLists.txt           ← SOC 构建定义
│   ├── config/
│   │   └── domain_config.yaml   ← CAN/UART/SOMEIP 运行时配置
│   │
│   ├── platform/                ← ① 平台层
│   │   ├── ara/core/              基础类型（ErrorCode, Optional, Result）
│   │   ├── ara/exec/              执行管理
│   │   ├── ara/log/               日志模块
│   │   └── Spi/                   SPI 驱动封装
│   │
│   ├── communication/           ← ② 通信层
│   │   ├── ara/com/              SOME/IP 服务通信（Proxy, Skeleton, SD）
│   │   └── SpiGateway/           SPI 帧处理（解码 MCU 发来的 SPI 帧）
│   │
│   ├── diag/                    ← ③ 诊断层
│   │   └── ara/diag/             UDS 诊断服务
│   │
│   └── apps/DomainController/   ← ④ 应用层
│       include/ src/              ConfigManager + SignalFusion + main
│
├── proto/                       ← 通信协议定义
│   └── signal_gateway.proto     ← MCU↔SOC SPI 帧 Protobuf 定义
│
└── tools/                       ← 工具
    └── uart_logger/             ← UART 日志工具
```

---

## 🚀 快速开始

### 环境准备

```bash
# MCU 交叉编译工具链
sudo apt install gcc-arm-none-eabi make

# SOC 构建工具
sudo apt install cmake g++

# 验证
arm-none-eabi-gcc --version
cmake --version
```

### 编译 MCU 端

```bash
cd mcu && make
# 生成产物: mcu_demo.elf / mcu_demo.hex / mcu_demo.bin
```

### 烧录 MCU

```bash
# 需要 J-Link 调试器连接 S32K144 开发板
cd mcu && make flash
```

### CAN 通信验证

**1. Ubuntu 端 — 启动 CAN 接口**

```bash
# 验证 CANable 设备已识别
lsusb -d 1d50:606f

# 启动 can0 接口 (500kbps)
./tools/can_setup.sh up

# 监控 CAN 总线流量
./tools/can_setup.sh monitor
```

**2. MCU 端 — 上电运行**

烧录后 MCU 以 500kbps 收发 CAN 消息（中断驱动 RX/TX）：
- TX CAN ID: `0x123`（CanTp SF/MF 交替），RX CAN ID: `0x100`
- 架构: 多路 CAN (HTH 编码 Controller+MB, hth 进 CanIf 配置表), 分层回调 (CanIf_McalRxCallback / CanIf_McalTxCallback), TX 确认链 (Can → CanIf → PduR → CanTp)
- LED: 绿闪=心跳, 橙=TX, 红=错误（蓝灯无功能，仅诊断时临时使用）

**3. 预期输出**

```
candump can0:
  (000.000) can0 123#0100AA55AA550000
  (001.749) can0 123#0200AA55AA550000
  (001.749) can0 123#0300AA55AA550000
```

**4. 双向通信测试 (MCU ↔ Ubuntu)**

```bash
# Ubuntu → MCU: 发送 FF 多帧首帧 (CAN ID 0x100, 声明总长 20 字节)
cansend can0 100#1014010203040506
# MCU 收到 FF 后自动回 FC 流控帧 (ISO 15765-2) — candump 应看到:
#   can0 100 [8] 10 14 01 02 03 04 05 06   ← 我们发的 FF
#   can0 100 [8] 30 08 01 AA AA AA AA AA   ← MCU 回的 FC (CTS, BS=8, STmin=1)
# 继续发 CF 完成重组:
cansend can0 100#210708090A0B0C0D
cansend can0 100#220E0F1011121314
```

### 编译 SOC 端

```bash
cd soc && mkdir -p build && cd build
cmake .. && make -j$(nproc)
# 生成产物: soc/build/domain_controller_soc
```

### 运行 SOC 端

```bash
./soc/build/domain_controller_soc -c soc/config/domain_config.yaml
```

---

## 📖 文档索引

> 文档按学习顺序排列，适合有 C/C++ 基础的读者从上往下阅读。

| 序号 | 文档 | 适合谁 | 内容 |
|------|------|--------|------|
| 1 | [MCU零基础学习计划](docs/MCU零基础学习计划.md) | 刚入门嵌入式 | 从零开始的阶段式学习路线 |
| 2 | [VehicleGateway_Design.md](docs/VehicleGateway_Design.md) | 想理解整体架构 | SPI 协议、CAN 信号矩阵、软件分层 |
| 3 | [AUTOSAR_学习路线图](docs/AUTOSAR_学习路线图.md) | 想理解 AUTOSAR | CP/AP 概念与代码对应关系 |
| 4 | [从零学CAN](docs/从零学CAN.md) | 没接触过 CAN | CAN 协议原理 + FlexCAN SDK 开发 |
| 5 | [S32K144_DRV_层开发指南](docs/S32K144_DRV_层开发指南.md) | 需要查 SDK API | DRV 层外设开发流程和 API 速查 |
| 6 | [MCU_交叉编译与烧录指南](docs/MCU_交叉编译与烧录指南.md) | 需要烧录/调试 | 工具链搭建 + J-Link 烧录流程 |
| 7 | [AUTOSAR CAN 栈系列](docs/can/CAN_AUTOSAR/1_AUTOSAR_CP_CAN通信栈.md) | 正在学 CAN 栈 | 5 篇从零讲解：通信栈全景 → Can 驱动 → CanIf → N-PDU → CanTp |
| 8 | [EcuM 零基础入门](docs/EcuM/EcuM_零基础入门.md) | 想理解启动流程 | EcuM 初始化调度 + MainFunction 周期驱动 |

---

## 🔌 硬件要求

| 硬件 | 用途 | 是否必需 |
|------|------|---------|
| S32K144 开发板 | MCU 运行平台 | ✅ 是 |
| J-Link / OpenSDA 调试器 | 烧录和调试 | ✅ 是 |
| FT2232H USB-SPI 桥 | MCU↔SOC SPI 通信 | ✅ 是 |
| USB↔CAN 工具 | CAN 通信验证 | ✅ 是 |
| 按键模块 | 模拟 GPIO 信号源 | ✅ 是 |

---

## 📄 License

本项目采用 **Apache 2.0** 协议开源，详见 [LICENSE](LICENSE) 文件。

---

> **一句话总结**
>
> GPIO 物理信号 → MCU CP 六层处理（MCAL→ECU Abstraction→Services→RTE→SWC）→ SPI / CAN / UART 输出
> → SOC AP 三层处理（Platform→Communication→App）→ SOME/IP + CAN 发布。
> 从寄存器到服务，逐层递进，模拟真实车载域控制器链路。
