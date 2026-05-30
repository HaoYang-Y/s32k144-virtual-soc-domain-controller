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
> S32K144 开发板采集 GPIO 按键 + ADC 旋钮，通过 SPI 传输给 Ubuntu 虚拟机，
> SOC 端差分解码后通过 SOME/IP 服务和 CAN 帧发布到其他域控制器。
> 代码按 AUTOSAR CP 三层（MCAL/Com Stack/SWC）和 AP 三层（Platform/Service/Communication）组织，
> 模拟真实车载域控制器架构。

---

## 📋 目录

- [学习目标](#-学习目标)
- [项目架构](#-项目架构)
- [学习路线](#-学习路线)
- [项目结构](#-项目结构)
- [快速开始](#-快速开始)
- [License](#-license)

---

## 🎯 学习目标

| 阶段 | 目标 | 关联 AUTOSAR 概念 |
|------|------|------------------|
| **一：MCU MCAL 层** | GPIO/ADC/PWM 驱动（直接寄存器操作） | CP MCAL: Gpio, Adc, Pwm, Mcu |
| **二：MCU 信号采集** | 按键中断 + ADC 定时采样 + 共享缓冲区 | CP SWC, RTE（volatile 共享内存模拟） |
| **三：CAN 通信** | FlexCAN 收发 + DBC 信号编解码 | CP Can, CanIf, PduR, Com |
| **四：MCU↔SOC SPI 通信** | SPI 32B 固定帧 + 差分协议 + CRC8 | CP Spi, Com Stack |
| **五：SOME/IP 服务通信** | vsomeip Service/Event 发布 + SD 发现 | AP ara::com, ara::sd, E2E |
| **六：UDS 诊断（后续）** | ISO 14229 诊断协议 | Dcm, Dem |

### 适合谁？

- **有 Linux C++ 基础，想入门车载嵌入式** — 从 MCU MCAL 层开始，理解硬件原理
- **想系统学习 CAN / SOME/IP 通信** — 双主线递进，理论基础 + 动手实践
- **对 AUTOSAR CP / AP 概念感兴趣** — 代码按 AUTOSAR 分层组织，建立全局视野

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
    ├── GPIO 按键 ×9（车门×4 / 档位×4 / 喇叭）
    └── ADC 旋钮 ×2（方向盘角度 / 车速）
```

### 数据流

```
  GPIO 按键 ──中断──┐
  ADC 旋钮 ──PIT定时─┤
                     ▼
              MCU 共享缓冲区（volatile struct）
                     │
              SWC 周期调度
                     │
              Com Stack（差分编码 + Pdu 编解码）
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
  MCU 端（裸机 C，AUTOSAR CP 三层）       SOC 端（C++ 守护进程，AUTOSAR AP 三层）
  ┌──────────────────────────────┐       ┌─────────────────────────────────┐
  │ signal_app/  (SWC)           │       │ comm/ (CommunicationService)    │
  │   VehicleSpeed_SWC_Run()     │       │   vsomeip Events 0x8001~0x8007 │
  │   SteeringAngle_SWC_Run()    │       │   ara::com 风格 Stub 接口       │
  │   BodySignals_SWC_Run()      │       ├─────────────────────────────────┤
  ├──────────────────────────────┤       │ svc/ (SignalBridgeService)      │
  │ signal_com/  (Com Stack+RTE) │       │   SignalBuffer（无锁环形缓冲）    │
  │   ComStack: 信号路由+差分编码 │       │   DiffCodec（差分帧解析）        │
  │   CanDrv:   CAN 帧编解码     │  SPI  ├─────────────────────────────────┤
  │   SpiDrv:   SPI 帧编解码     │◄─────►│ platform/ (SpiDevice)           │
  │   com_stub.h: RTE 宏         │ 32B帧 │   libmpsse 封装                  │
  ├──────────────────────────────┤       └─────────────────────────────────┘
  │ mcal/      (MCAL)            │
  │   GpioDrv / AdcDrv / PwmDrv  │
  │   McuDrv / CanDrv / SpiDrv   │
  └──────────────────────────────┘
```

---

## 📚 学习路线

> **👉 详细学习计划见 [docs/MCU零基础学习计划.md](docs/MCU零基础学习计划.md)**
> **👉 设计方案见 [docs/VehicleGateway_Design.md](docs/VehicleGateway_Design.md)**

### 推荐学习顺序

```
第一阶段：MCU MCAL 基础（GPIO→ADC→PWM→Mcu→CAN→SPI）
         ↓        附 AUTOSAR CP MCAL 概念
第二阶段：MCU 信号采集（按键中断 + ADC 定时采样 + 共享缓冲区）
         ↓        附 CP RTE / SWC 概念
第三阶段：CAN 通信精进（FlexCAN 收发 + DBC 编解码）
         ↓        附 CP Can/CanIf/PduR/Com 概念
第四阶段：SPI 通信（32B 固定帧 + 差分协议 + CRC8）
         ↓        附 CP Spi/Com Stack 概念
第五阶段：SOME/IP 服务通信（vsomeip + SD + Event）
         ↓        附 AP ara::com/ara::sd 概念
第六阶段：UDS 诊断（后续）
```

---

## 📁 项目结构

```
s32k144-virtual-soc-domain-controller/
│
├── README.md                   ← 项目说明
├── LICENSE                     ← Apache 2.0 开源协议
├── Dockerfile                  ← Docker 开发环境（可选）
│
├── docs/                       ← 文档
│   ├── VehicleGateway_Design.md   ← ★ 架构设计（真相来源）
│   ├── MCU零基础学习计划.md       ← ★ 学习计划
│   ├── AUTOSAR_学习路线图.md      ← AUTOSAR CP/AP 概念对照
│   ├── S32K144_DRV_层开发指南.md  ← NXP SDK DRV 层 API 参考
│   ├── MCU_交叉编译与烧录指南.md  ← 工具链 + J-Link 烧录
│   ├── 从零学CAN.md              ← CAN 协议教程
│   ├── TaskPlan.md              ← 任务跟踪
│   └── usbcan_device_info.md    ← USB-CAN 设备信息
│
├── config/
│   └── domain_config.yaml      ← CAN/UART/SOMEIP 配置
│
├── mcu/                        ← ★ MCU 端（AUTOSAR CP）
│   ├── mcal/                   ← MCAL 层（直接寄存器操作）
│   │   ├── GpioDrv.h / GpioDrv.c     GPIO 驱动
│   │   ├── McuDrv.h / McuDrv.c       MCU 时钟
│   │   ├── AdcDrv.h / AdcDrv.c       ADC 驱动（待实现）
│   │   └── PwmDrv.h / PwmDrv.c       PIT 定时器（待实现）
│   ├── signal_com/             ← Com Stack + RTE（待实现）
│   ├── signal_app/             ← SWC 组件（待实现）
│   ├── src/main.c              ← 主程序
│   ├── S32_SDK_S32K1xx_RTM_4.0.2/ ← NXP 官方 S32 SDK
│   └── Makefile                ← MCU 构建文件
│
├── soc/                        ← SOC 端（AUTOSAR AP）（待重构为 mpu/）
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── can_manager/        ← SocketCAN（调试用）
│   │   ├── signal_fusion/      ← 信号解析与融合
│   │   ├── someip_service/     ← SOME/IP
│   │   └── uds_server/         ← UDS 诊断（预留）
│   └── config/
│
├── tools/
│   ├── soc_build.sh            ← SOC 端构建脚本
│   └── mcu_flash.sh            ← MCU 烧录脚本
│
└── scripts/
```

---

## 🚀 快速开始

### 第一步：安装工具链

```bash
sudo apt install gcc-arm-none-eabi make
```

### 第二步：编译 MCU 端

```bash
cd mcu && make
```

### 第三步：烧录 MCU

```bash
# 需要 J-Link 调试器连接 S32K144 开发板
cd mcu && make flash
```

### 第四步：编译 SOC 端

```bash
cd soc && mkdir -p build && cd build
cmake .. && make
```

## 🔌 硬件要求

| 硬件 | 用途 | 是否必需 |
|------|------|---------|
| S32K144 开发板 | MCU 运行平台 | ✅ 是 |
| J-Link / OpenSDA 调试器 | 烧录和调试 | ✅ 是 |
| FT2232H USB-SPI 桥 | MCU↔SOC SPI 通信 | ✅ 是 |
| USB↔CAN 工具 | CAN 通信验证 | ✅ 是 |
| 按键/旋钮模块 | 模拟物理信号源 | ✅ 是 |

---

## 📄 License

本项目采用 **Apache 2.0** 协议开源，详见 [LICENSE](LICENSE) 文件。

---

> **一句话总结**
>
> GPIO/ADC 物理信号 → MCU CP 三层处理 → SPI 差分帧 → SOC AP 三层处理 → SOME/IP + CAN 发布。
> 从寄存器到服务，逐层递进，模拟真实车载域控制器链路。
