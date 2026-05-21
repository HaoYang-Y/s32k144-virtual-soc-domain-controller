# s32k144-virtual-soc-domain-controller — 基于 S32K144 与虚拟 SOC 的域控制器学习工程

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/MCU-S32K144-blue)](.)
[![Build](https://img.shields.io/badge/Build-Makefile%20%7C%20CMake-green)](.)
[![GitHub Stars](https://img.shields.io/github/stars/HaoYang-Y/s32k144-virtual-soc-domain-controller?style=social)](https://github.com/HaoYang-Y/s32k144-virtual-soc-domain-controller)

> **仓库地址**
>
> [![GitHub](https://img.shields.io/badge/GitHub-HaoYang--Y/s32k144--virtual--soc--domain--controller-181717?logo=github)](https://github.com/HaoYang-Y/s32k144-virtual-soc-domain-controller)
> [![Gitee](https://img.shields.io/badge/Gitee-Fighter--CTN/s32k144--virtual--soc--domain--controller-C71D23?logo=gitee)](https://gitee.com/Fighter-CTN/s32k144-virtual-soc-domain-controller)
>
> 如果这个工程对你有帮助，欢迎在 GitHub 上点一颗 ⭐ Star，让更多人看到！

> **从 S32K144 MCU 到 Ubuntu 虚拟 SOC，逐层递进掌握车载通信协议栈**  
>
> 真实的 S32K144 MCU + 虚拟的 Ubuntu SOC，MCU 接收 CAN 信号、
> 解析融合后通过 UART 传输给 SOC，SOC 将结构化信号封装为
> SOME/IP 服务发送到其他域控制器——模拟真实车载域控制器架构。
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
| **一：MCU 硬件基础** | 6 大外设寄存器编程 | — |
| **二：CAN 通信精进** | FlexCAN 驱动 + CAN 信号解析 + UART 传输 | MCAL Can, CanIf, PduR, Com, CanNm |
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
┌──────────────────────────────────────────────────────────────────┐
│                       本工程数据流（学习简化版）                    │
│                                                                  │
│  CAN 总线 ──CAN帧──► MCU (S32K144)                              │
│                        │                                         │
│                        ├─ FlexCAN 接收 CAN 帧                    │
│                        ├─ CAN 信号解析 (DBC 映射)                │
│                        └─ UART 发送结构化信号                     │
│                            │                                     │
│                            │ USB-UART 线                         │
│                            ▼                                     │
│                        SOC (Ubuntu 虚拟机)                       │
│                        ├─ UART 接收                              │
│                        ├─ 信号反序列化 / 校验                     │
│                        ├─ CAN 信号融合                           │
│                        ├─ SOME/IP 序列化 + 服务发现               │
│                        └─ 车载以太网 ──► 其他域控制器             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘

> 💡 **与实际工程的区别**
>
> 真实域控制器中，MCU 与 SOC 之间通常通过 **SPI 总线**（LPSPI）进行通信，
> 以满足更高的带宽和实时性要求。本工程使用 **UART** 替代 SPI，
> 原因如下：
>
> | 方面 | 实际工程（SPI） | 本工程（UART） |
> |------|----------------|---------------|
> | 通信速度 | 最高数十 Mbps | 最高 115200~921600 bps，足够教学演示 |
> | 接线方式 | SCK + MOSI + MISO + CS（4 线） | TX + RX（2 线），入门友好 |
> | 主机依赖 | SPI 需主机（MCU 或 SOC 一方做主） | UART 无需主从，两端对等，调试简单 |
> | 硬件工具 | USB↔SPI 工具不易获取 | **USB↔UART 线容易买到** |
>
> **如果你有 USB↔SPI 工具**（如 FT4232H 等），可以对比学习两种通信方式。
> MCU 端的 LPSPI 模块已预留，详见 [docs/MCU零基础学习计划.md](docs/MCU零基础学习计划.md) 扩展部分。
```

### 协议栈分层

```
   AUTOSAR AP 层            ara::com / ara::sd / E2E
        ↕                        SOME/IP 协议
   SOC UART 层            Socket 串口接收 / 自定义传输协议
        ↕                        UART 物理层 (USB↔UART)
   MCU UART 层            LPUART 驱动
        ↕                        结构化信号 (解析后)
   MCU 信号层             CAN 信号解析 / DBC 映射
        ↕                        CAN 帧
   MCAL 层                FlexCAN 寄存器驱动 / MCAL Can
        ↕
   硬件层                 S32K144 CAN 收发器 / CAN 总线
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
  ├ MCU 端 CAN 信号解析 (DBC)
  ├ MCU 端 UART 传输协议
  ├ SOC 端 UART 接收 + 信号融合
  └ CAN 网络管理 (CanNm)
         ↓
第三阶段：SOME/IP 服务通信 ──── 附 AUTOSAR AP 概念
  ├ SOME/IP 协议基础
  ├ SOME/IP 服务发现 (SD)
  ├ SOME/IP 序列化
  └ UART → SOME/IP 联调
         ↓
第四阶段：UDS 诊断 (后续)
```

---

## 📁 项目结构

```
s32k144-virtual-soc-domain-controller/
│
├── README.md                  ← 项目说明
├── LICENSE                    ← Apache 2.0 开源协议
├── Dockerfile                 ← Docker 开发环境（可选）
│
├── docs/
│   ├── MCU零基础学习计划.md    ← ★ 详细学习计划，从这里开始
│   └── AUTOSAR_学习路线图.md   ← AUTOSAR 概念对照指南
│
├── config/
│   └── domain_config.yaml     ← CAN/UART/SOMEIP 配置
│
├── mcu/                       ← ★ MCU 端核心代码
│   ├── s32k144_flash.ld       ← 链接脚本（共享）
│   ├── gpio/                  ← GPIO 模块（书籍第 4 章）
│   ├── uart/                  ← UART 模块（书籍第 5 章）
│   ├── timer/                 ← 定时器模块（书籍第 7/9 章）
│   ├── adc/                   ← ADC 模块（书籍第 8 章）
│   ├── flexcan/               ← ★ FlexCAN 模块（书籍第 10 章）
│   ├── clock/                 ← 时钟模块（书籍第 3 章）
│   └── domain_controller/     ← ★ 域控制器应用（FlexCAN + UART 联调，待实现）
│
├── soc/                       ← SOC 端（TODO 骨架）
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── config_manager.h   ← 配置管理器
│   │   ├── can_manager.h      ← CAN 总线管理器（调试用，可选）
│   │   ├── uart_receiver.h    ← ★ UART 接收管理器（新增）
│   │   └── signal_manager.h   ← 信号融合中心
│   ├── src/
│   │   ├── main.cpp           ← 主入口（TODO 占位）
│   │   ├── can_manager/       ← SocketCAN（调试用，可选）
│   │   ├── uart_receiver/     ← ★ UART 接收（新增）
│   │   ├── signal_fusion/     ← 信号解析与融合
│   │   ├── someip_service/    ← SOME/IP（阶段三）
│   │   ├── uds_server/        ← UDS 诊断（预留）
│   │   └── web_api/           ← Web API（预留）
│
└── tools/
    ├── soc_build.sh           ← SOC 端构建脚本
    └── mcu_flash.sh           ← MCU 烧录脚本
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

### 第三步：CAN + UART 联调

```bash
# MCU 端：编译 domain_controller 应用
cd mcu/domain_controller && make

# SOC 端：编译 UART 接收 + 信号融合
cd soc && mkdir -p build && cd build
cmake .. && make

# SOC 端启动接收（USB-UART 接入）
./soc_app /dev/ttyUSB0 115200
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

本项目采用 **Apache 2.0** 协议开源，详见 [LICENSE](LICENSE) 文件。
衍生作品须保留 [NOTICE](NOTICE) 文件中的出处声明（包含本项目的 GitHub 地址）。

---

> **一句话总结**
>
> CAN 总线 → MCU 接收解析 → UART 传输 → SOC 融合 → SOME/IP 发布。
> 从寄存器到服务，逐层递进，学完每个模块形成完整域控制器链路。
