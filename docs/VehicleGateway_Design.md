# 车载信号网关设计文档 v3.0

> **定位**: 双芯架构原型，打通 MCU 物理信号 → SPI → CAN → SOME/IP 全链路
> **技术栈**: NXP S32K144 + Ubuntu 24.04 + SPI + CAN + vsomeip
> **设计原则**:
> 1. 零额外学习成本: 无 RTOS，不依赖商业 AUTOSAR 工具
> 2. 学做合一: 每个模块标注对应 AUTOSAR 概念
> 3. 架构映射而非框架引入: 裸机 C / Linux C++ 代码按 CP/AP 分层思想组织
> 
> **当前状态**: CAN 和 UART 尚未调通（2026-06-19），MCAL 各模块骨架代码已搭好。

---

## 1. 项目概述

| 域 | 硬件 | 职责 | 代码组织 | AUTOSAR 对应 |
|----|------|------|---------|-------------|
| MCU | NXP S32K144 | CAN 通信 + 信号采集 | 裸机 C，CP 四层模型 | CP: MCAL/ECU Abstraction/RTE/SWC |
| MPU | Ubuntu 24.04 VM | 信号处理 + SOME/IP 发布 | C++ 守护进程，AP 三层模型 | AP: Platform/Service/Communication |
| MCU↔MPU 通信 | FT2232H USB-SPI 桥 | SPI Master(MCU)→Slave(FT2232H) | 自定义协议 32B 固定帧 | CP: Spi + Com Stack |
| CAN 验证 | FlexCAN0 + USB-CAN | CAN 2.0B 报文收发 | DBC 信号矩阵 | CP: Can/CanIf/PduR |
| RTE 模拟 | Rte.h/c | SWC ↔ BSW 桥接 | volatile 共享内存，零开销 | CP: RTE |
| MCU 调试输出 | LPUART1 | 日志/状态打印 | CDD/Uart | CP: CDD |

---

## 2. 硬件拓扑

```
PC (宿主机)
├── Ubuntu 24.04 VM
│   ├── Signal Bridge Daemon (vsomeip + libmpsse)
│   └── FT2232H (USB 直通, SPI Slave)
│
└── S32K144 开发板
    ├── LPSPI0 ── SPI Master → FT2232H
    ├── FlexCAN0 ── CAN → USB-CAN 分析仪
    ├── LPUART1 ── UART → 宿主机串口（调试日志）
    ├── GPIO 按键 ×5 (车门×4 + 档位 P/R/N/D)
    └── ADC 旋钮 ×2 (方向盘角度 + 车速)
```

### 模拟信号源

| 信号 | 类型 | 模拟源 | 采样方式 | 周期 |
|------|------|--------|---------|------|
| 车门状态 ×4 | 开关量 | GPIO 按键 | 中断+扫描 | 100ms |
| 档位 P/R/N/D | 枚举 | GPIO 按键 | 中断 | 变化即报 |
| 大灯/转向灯/喇叭 | 开关/枚举 | GPIO 按键 | 中断 | 变化即报 |
| 方向盘角度 | 模拟量 | ADC 旋钮 | PIT 定时器 | 50ms |
| 车速 | 模拟量 | ADC 旋钮 | PIT 定时器 | 100ms |

---

## 3. SPI 通信协议

### 通信模型: MCU Master → FT2232H Slave, 固定 32B 帧, 1MHz → 0.256ms/次

### 帧格式
```
CMD(1B) | SIZE(1B) | PAYLOAD(28B) | CRC8(1B)
```

### 命令码

| 命令 | 值 | 说明 |
|------|----|------|
| CMD_SENSOR_POLL | 0xA0 | 全量信号 |
| CMD_HEARTBEAT | 0xA1 | 心跳 |
| CMD_DIFF_POLL | 0xA2 | ★ 差分 (推荐) |
| CMD_FULL_SYNC | 0xA3 | 强制全量 (初始化/恢复) |
| CMD_SET_OUTPUT | 0xA4 | 控制输出 (预留) |

### 差分传输规则

**sensor_mask (u32) 位 → 字段映射:**
| bit | 字段 | 大小 | bit | 字段 | 大小 |
|-----|------|------|-----|------|------|
| 0 | door_status | 1B | 4 | horn | 1B |
| 1 | gear_position | 1B | 5 | steering_angle | 2B |
| 2 | headlight | 1B | 6 | vehicle_speed | 2B |
| 3 | turn_signal | 1B | 7-31 | 预留 | — |

**规则:**
- mask bit=1 表示该字段有变化，按 bit 序依次打包
- 接收端维护全量状态缓存，差分帧仅更新变化字段
- 物理层固定 32B，差分节省逻辑有效载荷

**混合策略:** 默认差分 | 每 5s 插入全量同步 | CRC 异常时主动请求 CMD_FULL_SYNC

**CRC:** CRC-8-ATM (多项式 0x07)，校验 CMD+SIZE+PAYLOAD 共 30B

---

## 4. CAN 信号矩阵

| 信号 | CAN ID | 周期 | 位域 | 范围 | 步长 |
|------|--------|------|------|------|------|
| 车门状态 | 0x100 | 100ms | B0[0..3] | 0/1 | — |
| 档位 | 0x110 | 变化 | B0[4..6] | 0=P,1=R,2=N,3=D | — |
| 大灯 | 0x120 | 变化 | B0[0..1] | 0=关,1=近光,2=远光 | — |
| 转向灯 | 0x121 | 变化 | B0[2..3] | 0=关,1=左,2=右 | — |
| 喇叭 | 0x130 | 变化 | B0[0] | 0/1 | — |
| 方向盘角度 | 0x200 | 50ms | B0..1(i16 LSB) | -450°~+450° | 0.1° |
| 车速 | 0x201 | 100ms | B0..1(u16 LSB) | 0~300 km/h | 0.01 km/h |

**CAN ID 分配:** 0x100 车身域 | 0x200 动力域

---

## 5. MCU 软件架构 (AUTOSAR CP)

### 完整分层

```
┌──────────────────────────────────────────────────────────┐
│  ⑤ App/Swc_SignalGateway (SWC)                           │
│     不直接操作硬件，通过 Rte_Read/Rte_Write 读写信号       │
│     main.c → 周期调度各 SWC                               │
├──────────────────────────────────────────────────────────┤
│  ④ RTE/                                                  │
│     #define Rte_Read(sig)  g_shared_signals.sig          │
│     #define Rte_Write(sig, val) g_shared_signals.sig=val │
│     编译期展开，零抽象开销，volatile 共享内存              │
├──────────────────────────────────────────────────────────┤
│  ③ EcuAbstraction/                                       │
│     CanIf.h/c  — CAN 接口抽象 (CanIf_Transmit 等)        │
│     IoHwAb.h/c — IO 硬件抽象 (IoHwAb_ReadPin 等)         │
│     SpiIf.h/c  — SPI 接口抽象 (SpiIf_WriteIb 等)         │
├──────────────────────────────────────────────────────────┤
│  ④ Services/                                             │
│     Com.h/c   — 信号↔PDU 编解码 (Com_SendSignal 等)     │
│     PduR.h/c  — PDU 路由                                  │
│     CanTp.h/c — CAN 传输层 (ISO 15765-2)                 │
│     BswM.h/c  — 模式管理                                  │
│     EcuM.h/c  — ECU 状态管理                              │
│     Log.h/c   — 日志服务                                  │
├──────────────────────────────────────────────────────────┤
│  ② CDD/                                                  │
│     Uart.h/c — LPUART 驱动（调试日志输出）                 │
├──────────────────────────────────────────────────────────┤
│  ① MCAL/（每模块 include/ src/ config/ 三目录）           │
│     Gpio.h/c  — Gpio_ReadPin / Gpio_WritePin             │
│     Mcu.h/c   — Mcu_InitClock / Mcu_GetCoreFreq           │
│     Adc.h/c   — Adc_ReadGroup / Adc_Init                 │
│     Can.h/c   — Can_Transmit / Can_Receive               │
│     Spi.h/c   — Spi_WriteIb / Spi_ReadIb                 │
│     Port.h/c  — Port_SetPinMode / Port_SetMuxMode        │
├──────────────────────────────────────────────────────────┤
│  NXP S32 SDK (S32K144 芯片头文件 + 启动代码 + 链接脚本)    │
└──────────────────────────────────────────────────────────┘
```

### 核心设计决策

1. **RTE = 宏 + volatile 共享内存**: `Rte_Read(sig)` → `g_shared_signals.sig`，编译期展开，零开销
2. **共享缓冲区**: 所有信号在 `volatile struct { u16 speed; i16 angle; u8 door,gear,light,turn,horn; } g_shared_signals;` ISR 只写，主循环只读，无锁
3. **接口命名 100% AUTOSAR 标准**: `Can_Transmit()`, `Rte_Write()`, `Gpio_ReadPin()`, `Spi_WriteIb()`
4. **MCAL 使用 NXP SDK API**: 不直接操作寄存器，通过 `PINS_DRV_*`、`FLEXCAN_DRV_*`、`LPUART_DRV_*` 等 SDK API 开发

### 执行模型

```
main():
  Mcu_InitClock → Port_Init → Gpio_Init → Adc_Init → Spi_Init → Can_Init
  while(1):
    Swc_SignalGateway_Run()   (周期调度: 50ms/100ms)
      ├── Rte_Read(xxx)
      ├── 信号处理
      └── Rte_Write(yyy)
    __WFI()

中断 (仅调 MCAL, <1ms):
  PIT_IRQHandler(50ms):  Adc_ReadGroup → 更新共享缓冲区
  GPIO_IRQHandler:       Gpio_ReadPin → 更新共享缓冲区
```

---

## 6. MPU 软件架构 (AUTOSAR AP)

### 分层映射

```
┌──────────────────────────────────────────────────────────┐
│  ④ apps/DomainController (Application)                   │
│     ConfigManager — 读取 domain_config.yaml              │
│     SignalFusion  — 信号融合与状态管理                    │
│     main.cpp      — 启动入口                              │
├──────────────────────────────────────────────────────────┤
│  ③ communication/                                        │
│     ara/com/   — SOME/IP 服务通信                        │
│       InstanceIdentifier, Proxy, Skeleton,               │
│       ServiceDiscovery, SomeIpBinding                     │
│     SpiGateway/ — SPI 帧解码与路由                        │
├──────────────────────────────────────────────────────────┤
│  ② platform/                                             │
│     ara/core/  — ErrorCode, Optional, Result            │
│     ara/exec/  — 执行管理                                │
│     ara/log/   — 日志模块                                │
│     Spi/       — SPI 驱动封装 (libmpsse)                 │
├──────────────────────────────────────────────────────────┤
│  ① diag/ara/diag/                                        │
│     UdsServer — ISO 14229 诊断服务 (预留)                 │
└──────────────────────────────────────────────────────────┘
```

### SOM/IP Event 定义

| Event ID | 信号 | 类型 | Event ID | 信号 | 类型 |
|----------|------|------|----------|------|------|
| 0x8001 | 车门状态 | u8 | 0x8005 | 喇叭 | u8 |
| 0x8002 | 档位 | u8 | 0x8006 | 方向盘角度 | i16 |
| 0x8003 | 大灯 | u8 | 0x8007 | 车速 | u16 |
| 0x8004 | 转向灯 | u8 | | | |

### 核心设计决策

1. **Platform 封装硬件**: `SpiDriver` 封装 SPI 操作，业务层只调 `ReadFrame()`
2. **信号订阅模式**: `Get()` 同步获取最新值 | `Subscribe(ms, cb)` 变化通知
3. **无锁环形缓冲区**: SPSC 模型，SPI Reader 写，Publisher 读

---

## 7. 端到端数据流

**核心结论:** 端到端总延迟 ≤ 50ms，瓶颈为 SPI 轮询间隔

**数据路径:** MCAL ISR → RTE(零开销) → SWC → CanIf/SpiIf → SPI(32B 固定帧) → Platform ReadFrame → SpiGateway 解码 → SignalFusion → SOME/IP notify

---

## 8. 项目目录结构（当前实际结构）

```
s32k144-virtual-soc-domain-controller/
│
├── mcu/                          ← AUTOSAR CP
│   ├── MCAL/                     ← ① 微控制器抽象层
│   │   ├── Gpio/   (include/ src/ config/)
│   │   ├── Mcu/    (include/ src/ config/)
│   │   ├── Adc/    (include/ src/ config/)
│   │   ├── Can/    (include/ src/ config/)
│   │   ├── Spi/    (include/ src/ config/)
│   │   └── Port/   (include/ src/ config/)
│   ├── EcuAbstraction/           ← ② ECU 抽象层
│   │   ├── CanIf/  (include/ src/ config/)
│   │   ├── IoHwAb/ (include/ src/ config/)
│   │   └── SpiIf/  (include/ src/ config/)
│   ├── CDD/Uart/                 ← ③ 复杂驱动
│   ├── RTE/                      ← ④ 运行时环境
│   ├── App/Swc_SignalGateway/    ← ⑤ SWC 应用层
│   ├── include/                  ← 公共头文件 (Std_Types.h, Platform_Types.h)
│   ├── S32_SDK_S32K1xx_RTM_4.0.2/ ← NXP SDK
│   └── Makefile
│
├── soc/                          ← AUTOSAR AP
│   ├── platform/                 ← ① 平台层
│   │   ├── ara/core/ (ErrorCode, Optional, Result)
│   │   ├── ara/exec/ (ExecClient)
│   │   ├── ara/log/  (Logger)
│   │   └── Spi/      (SpiDriver)
│   ├── communication/            ← ② 通信层
│   │   ├── ara/com/  (Proxy, Skeleton, ServiceDiscovery)
│   │   └── SpiGateway/ (SpiGateway)
│   ├── diag/ara/diag/            ← ③ 诊断层 (UdsServer)
│   ├── apps/DomainController/    ← ④ 应用层
│   ├── config/domain_config.yaml
│   └── CMakeLists.txt
│
├── proto/signal_gateway.proto    ← MCU↔SOC 通信协议定义
│
└── tools/
    └── uart_logger/
```

---

> **v3.0** 重构版 | 同步当前 MCU CP 四层 + SOC AP 三层实际目录结构 | 约 350 行
