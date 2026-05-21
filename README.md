# s32k144-virtual-soc-domain-controller — 基于 S32K144 与虚拟 SOC 的域控制器学习工程

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/MCU-S32K144-blue)](.)
[![Build](https://img.shields.io/badge/Build-Makefile%20%7C%20CMake-green)](.)

> **从 S32K144 MCU 到 Ubuntu 虚拟 SOC，逐层递进掌握车载通信协议栈**  
>
> 真实的 S32K144 MCU + 虚拟的 Ubuntu SOC，通过 CAN 总线通信，
> 从 FlexCAN 寄存器驱动到 SocketCAN、再到 SOME/IP 服务通信，
> 模拟真实车载域控制器的软硬协同架构。
>
> 每层通信都附 AUTOSAR CP / AP 概念穿插，形成完整知识体系。

---

## 📋 目录

- [学习目标](#-学习目标)
- [项目架构](#-项目架构)
- [学习路线](#-学习路线)
- [项目结构](#-项目结构)
- [快速开始](#-快速开始)
- [配套书籍说明](#-配套书籍说明)
- [License](#-license)

---

## 🎯 学习目标

| 阶段 | 目标 | 关联 AUTOSAR 概念 |
|------|------|------------------|
| **一：MCU 硬件基础** | 5 大外设寄存器编程 | — |
| **二：CAN 通信精进** | FlexCAN 驱动 + SocketCAN + CAN 信号解析 | MCAL Can, CanIf, PduR, Com, CanNm |
| **三：SOME/IP 服务通信** | SOME/IP 协议 + 服务发现 + 序列化 | ara::com, ara::sd, E2E |
| **四：UDS 诊断（后续）** | ISO 14229 诊断协议 | Dcm, Dem |

### 适合谁？

- **有 Linux C++ 基础，想入门车载嵌入式** — 从 MCU 端开始，理解硬件原理
- **想系统学习 CAN / SOME/IP 通信** — 双主线递进，理论基础 + 动手实践
- **正在读《汽车电子S32K系列微控制器》** — 配套工程，边看书边动手
- **对 AUTOSAR CP / AP 概念感兴趣** — 每阶段穿插概念对照，建立全局视野

---

## 🏗 项目架构

```
┌──────────────────────────────────────────────────────────┐
│                    笔记本电脑 (SOC 端)                     │
│  ┌───────────────────────────────────────────────────┐    │
│  │   Domain Controller App (TODO 骨架 - 待你实现)     │    │
│  │   ├── ConfigManager   ← YAML 配置加载             │    │
│  │   ├── CanBus          ← SocketCAN 收发            │    │
│  │   ├── SignalFusion    ← CAN 信号解析与融合         │    │
│  │   ├── SomeIpServer    ← SOME/IP 服务 (预留)       │    │
│  │   └── UdsServer       ← UDS 诊断 (预留)           │    │
│  └───────────────────────────────────────────────────┘    │
│        │ CAN (USB↔CAN)                                    │
│        │ UART (USB↔UART)                                  │
└────────┼──────────────────────────────────────────────────┘
         │
┌────────▼──────────────────────────────────────────────────┐
│           S32K144 MCU 节点（你动手实现的部分）             │
│                                                          │
│  6 大外设模块（独立目录，独立测试）                          │
│  ┌──────┐ ┌──────┐ ┌───────┐ ┌─────┐ ┌────────┐ ┌─────┐  │
│  │ GPIO │ │ UART │ │ TIMER │ │ ADC │ │ FlexCAN│ │Clock│  │
│  │驱动  │ │驱动  │ │PIT/FTM│ │驱动  │ │驱动    │ │系统  │  │
│  └──────┘ └──────┘ └───────┘ └─────┘ └────────┘ └─────┘  │
│                                                          │
│  每个模块: header + source + main + Makefile              │
│  独立编译，独立烧录，独立验证                              │
└──────────────────────────────────────────────────────────┘
```

### 协议栈分层

```
   AUTOSAR AP 层            ara::com / ara::sd / E2E
        ↕                        SOME/IP 协议
   AUTOSAR CP 层     PduR / Com / CanNm / CanIf
        ↕                        CAN 帧
   MCAL 层           FlexCAN 寄存器驱动 / MCAL Can
        ↕
   硬件层            S32K144 物理总线 / SocketCAN
```

---

## 📚 学习路线

> **👉 请先阅读 [docs/MCU零基础学习计划.md](docs/MCU零基础学习计划.md) 获得完整学习路线**

### 推荐学习顺序

```
第一阶段：MCU 硬件基础 (GPIO→UART→TIMER→ADC→Clock)
         ↓
第二阶段：CAN 通信精进 ───────── 附 AUTOSAR CP 概念
  ├ FlexCAN 寄存器驱动
  ├ SocketCAN SOC 端
  ├ CAN 信号解析 (DBC)
  └ CAN 网络管理 (CanNm)
         ↓
第三阶段：SOME/IP 服务通信 ──── 附 AUTOSAR AP 概念
  ├ SOME/IP 协议基础
  ├ SOME/IP 服务发现 (SD)
  └ SOME/IP 序列化
         ↓
第四阶段：UDS 诊断 (后续)
```

---

## 📁 项目结构

```
s32k144-virtual-soc-domain-controller/
│
├── README.md                  ← 项目说明
├── LICENSE                    ← MIT 开源协议
├── Dockerfile                 ← Docker 开发环境（可选）
│
├── docs/
│   ├── MCU零基础学习计划.md    ← ★ 详细学习计划，从这里开始
│   └── AUTOSAR_学习路线图.md   ← AUTOSAR 概念对照指南
│
├── config/
│   └── domain_config.yaml     ← CAN/SOMEIP/UDS 配置
│
├── mcu/                       ← ★ MCU 端核心代码
│   ├── s32k144_flash.ld       ← 链接脚本（共享）
│   ├── gpio/                  ← GPIO 模块（书籍第 4 章）
│   ├── uart/                  ← UART 模块（书籍第 5 章）
│   ├── timer/                 ← 定时器模块（书籍第 7/9 章）
│   ├── adc/                   ← ADC 模块（书籍第 8 章）
│   ├── flexcan/               ← ★ FlexCAN 模块（书籍第 10 章）
│   └── clock/                 ← 时钟模块（书籍第 3 章）
│
├── soc/                       ← SOC 端（TODO 骨架）
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── config_manager.h   ← 配置管理器
│   │   ├── can_manager.h      ← CAN 总线管理器
│   │   └── signal_manager.h   ← 信号融合中心
│   ├── src/
│   │   ├── main.cpp           ← 主入口（TODO 占位）
│   │   ├── can_manager/
│   │   │   └── can_manager.cpp
│   │   ├── signal_fusion/
│   │   │   ├── config_manager.cpp
│   │   │   └── signal_manager.cpp
│   │   ├── someip_service/    ← SOME/IP（预留）
│   │   ├── uds_server/        ← UDS 诊断（预留）
│   │   └── web_api/           ← Web API（预留）
│
└── tools/
    └── soc_build.sh           ← SOC 端构建脚本
```

---

## 🚀 快速开始

### 第一步：安装工具链

```bash
sudo apt install gcc-arm-none-eabi make
```

### 第二步：完成 MCU 硬件基础

```bash
# 测试 GPIO 模块编译
cd mcu/gpio && make

# 按照 docs/MCU零基础学习计划.md 逐一填充 TODO
```

### 第三步：CAN 通信学习

```bash
# 启动物理/虚拟 CAN
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# 编译 SOC 端
cd soc && mkdir -p build && cd build
cmake .. && make

# 监听 CAN 消息
candump vcan0
```

---

## 📖 配套书籍说明

本项目是 **《汽车电子S32K系列微控制器——基于ARM Cortex-M4F内核》**（苏勇 著）的配套工程。

### 章节对应关系

| 书籍章节 | 项目模块 | 学习重点 |
|---------|---------|---------|
| 第 1~2 章 | — | ARM Cortex-M4F 基础、S32K14x 概览 |
| 第 3 章 | `mcu/clock/` | 时钟树、SPLL 配置 |
| 第 4 章 | `mcu/gpio/` | GPIO 寄存器（PDOR/PSOR/PCOR/PDIR/PDDR） |
| 第 5 章 | `mcu/uart/` | LPUART 波特率计算、异步串行通信 |
| 第 7 章 | `mcu/timer/` | PIT 周期中断定时器 |
| 第 8 章 | `mcu/adc/` | 逐次逼近型 ADC、12bit 采样 |
| 第 9 章 | `mcu/timer/`（FTM） | PWM 输出、输入捕获 |
| 第 10 章 | `mcu/flexcan/` | ★ CAN 协议、FlexCAN 报文收发 |
| 第 11 章 | — | FreeRTOS（后续扩展） |

---

## 🔌 硬件要求

| 硬件 | 用途 | 是否必需 | 替代方案 |
|------|------|---------|---------|
| S32K144 开发板 | MCU 运行平台 | ✅ 是 | — |
| J-Link / OpenSDA 调试器 | 烧录和调试 | ✅ 是 | — |
| USB↔UART 线 | PC ↔ MCU 串口通信 | ⚠️ UART 模块必需 | — |
| USB↔CAN 工具 | PC ↔ MCU CAN 通信 | ⚠️ CAN 模块必需 | **vcan 模拟** |
| 杜邦线/面包板 | 连接外围电路 | ⚠️ 视需要而定 | — |

---

## 📄 License

本项目采用 MIT 协议开源 — 详见 [LICENSE](LICENSE) 文件。

---

> **一句话总结**
>
> 从 MCU 寄存器到 SOC 服务，从 CAN 帧到 SOME/IP——逐层递进，
> 每学一层通信就附带上层的 AUTOSAR 概念，形成完整车载通信知识体系。
