# 车载信号网关设计文档 v2.2 (极简开发参照版)

> **定位**: 双芯架构原型，打通 MCU 物理信号 → SPI → CAN → SOME/IP 全链路
> **技术栈**: NXP S32K144 + Ubuntu 24.04 + SPI + CAN + vsomeip
> **设计原则**:
> 1. 零额外学习成本: 无 RTOS，不依赖商业 AUTOSAR 工具
> 2. 学做合一: 每个模块标注对应 AUTOSAR 概念
> 3. 架构映射而非框架引入: 裸机/Linux 代码按 CP/AP 核心思想组织

---

## 1. 项目概述

| 域 | 硬件 | 职责 | 代码组织 | AUTOSAR 对应 |
|----|------|------|---------|-------------|
| MCU | NXP S32K144 | CAN 通信 + 信号采集 | 裸机 C，CP 三层模型 | CP: MCAL/Com/SWC |
| MPU | Ubuntu 24.04 VM | 信号处理 + SOME/IP 发布 | C++ 守护进程，AP 三层模型 | AP: Platform/Service/Communication |
| MCU↔MPU 通信 | FT2232H USB-SPI 桥 | SPI Master(MCU)→Slave(FT2232H) | 自定义协议 32B 固定帧 | CP: Spi + Com Stack |
| CAN 验证 | FlexCAN0 + USB-CAN | CAN 2.0B 报文收发 | DBC 信号矩阵 | CP: Can/CanIf/PduR |
| RTE 模拟 | com_stub.h 宏 | SWC ↔ Com 桥接 | volatile 共享内存，零开销 | CP: RTE |

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

### 分层映射

```
Application SWC       → signal_app/        VehicleSpeed_SWC, SteeringAngle_SWC, BodySignals_SWC
   不直接操作硬件       (VehicleSpeed_SWC.c  通过 Com_ReceiveSignal/Com_SendSignal 宏读写
   周期调度             SteeringAngle_SWC.c
                       BodySignals_SWC.c)

RTE (虚拟)            → signal_com/        #define Com_ReceiveSignal(sig) g_shared_signals.##sig
   编译期展开          com_stub.h           #define Com_SendSignal(sig,val) g_shared_signals.##sig = val
   volatile 共享内存                       零抽象开销

Com Stack             → signal_com/        ComStack: 信号路由 + Pdu 编解码 + 差分编码
   信号↔Pdu 路由       ComStack.c/h         CanDrv: Can_Transmit(pdu) | CAN 帧编解码
   CAN/SPI 编解码      CanDrv.c/h           SpiDrv: Spi_WriteIb(data) | SPI 帧编解码
                       SpiDrv.c/h

MCAL                  → mcal/              GpioDrv: Gpio_ReadPin(ch, &val) | GpioDrv_Init(cfg)
   仅此层操作寄存器     GpioDrv.c/h          AdcDrv: Adc_ReadGroup(grp, &buf) | AdcDrv_Init(cfg)
   AUTOSAR 标准命名    AdcDrv.c/h           PwmDrv: Pwm_SetPeriod(tmr, period) (PIT 定时器)
                       PwmDrv.c/h           McuDrv: Mcu_InitClock(cfg)
                       McuDrv.c/h
```

### 架构图

```
Application SWC: VehicleSpeed_Run / SteeringAngle_Run / BodySignals_Run
    ↓ Com_ReceiveSignal / Com_SendSignal (RTE 宏)
Com Stack: ComStack_ProcessSpi / ComStack_ProcessCan → Can_Transmit / Spi_WriteIb
    ↓
MCAL: Gpio_ReadPin / Adc_ReadGroup / Pwm_SetPeriod
    ↓
NXP SDK
```

### 核心设计决策

1. **RTE = 宏 + volatile 共享内存**: `Com_ReceiveSignal(s)` → `g_shared_signals.s`
2. **共享缓冲区**: 所有信号在 `volatile struct { u16 speed; i16 angle; u8 door,gear,light,turn,horn; } g_shared_signals;` 事件只写，主循环只读，无锁
3. **接口命名 100% AUTOSAR 标准**: `Can_Transmit()`, `Com_SendSignal()`, `Gpio_ReadPin()`, `Spi_WriteIb()`

### 执行模型

```
main():
  Mcu_InitClock → GpioDrv_Init → AdcDrv_Init → SpiDrv_Init → CanDrv_Init
  while(1):
    VehicleSpeed_SWC_Run()    (100ms 周期)
    SteeringAngle_SWC_Run()   (50ms 周期)
    BodySignals_SWC_Run()     (100ms 周期)
    ComStack_ProcessSpi()     (差分编码 + SPI 发送)
    ComStack_ProcessCan()     (Pdu 编码 + CAN 发送)
    __WFI()

中断 (仅调 MCAL, <1ms):
  PIT_IRQHandler(50ms):  Adc_ReadGroup → g_shared_signals
  GPIO_IRQHandler:       Gpio_ReadPin → 更新共享缓冲区
```

---

## 6. MPU 软件架构 (AUTOSAR AP)

### 分层映射

```
Communication (ara::com) → comm/                     CommunicationService
   SOME/IP Event 发布       CommunicationService.cpp   VehicleSpeed_Stub → Event 0x8007
   Service Discovery        各 Stub 类                  SteeringAngle_Stub → Event 0x8006
                                                       BodySignals_Stub → Events 0x8001-0x8005

Service (ara::core)       → svc/                     SignalBridgeService
   信号缓存 + 差分解码      SignalBridgeService.cpp    SignalBuffer: 无锁环形缓冲区, 16 槽位
   服务接口                SignalBuffer.h             DiffCodec: 差分帧解析 + 全量状态缓存
                           DiffCodec.h                API: Get() / Subscribe(period_ms, cb)

Platform (POSIX)          → platform/                SpiDevice: libmpsse 封装, ReadFrame/WriteFrame
   硬件抽象                SpiDevice.cpp/h            ThreadPool: std::thread 封装
                           ThreadPool.h               Logger
                           Logger.h
```

### 架构图

```
Communication: CommunicationService (Stubs → vsomeip Events)
    ↓ Get() / Subscribe()
Service: SignalBridgeService (SignalBuffer + DiffCodec)
    ↓ ReadFrame()
Platform: SpiDevice (libmpsse)
    ↓
libmpsse + vsomeip
```

### 核心设计决策

1. **Platform 封装硬件**: `SpiDevice` 封装 SPI 操作，业务层只调 `ReadFrame()`
2. **服务接口 = ara::com Field 模式**: `Get()` 同步获取最新值 | `Subscribe(ms, cb)` 变化通知
3. **无锁环形缓冲区**: SPSC 模型 (16 槽位)，SPI Reader 写，Publisher 读

### SOME/IP Event 定义

| Event ID | 信号 | 类型 | Event ID | 信号 | 类型 |
|----------|------|------|----------|------|------|
| 0x8001 | 车门状态 | u8 | 0x8005 | 喇叭 | u8 |
| 0x8002 | 档位 | u8 | 0x8006 | 方向盘角度 | i16 |
| 0x8003 | 大灯 | u8 | 0x8007 | 车速 | u16 |
| 0x8004 | 转向灯 | u8 | | | |

---

## 7. 端到端数据流

**核心结论:** 端到端总延迟 ≤ 50ms，瓶颈为 SPI 轮询间隔

**数据路径:** MCAL 中断 → RTE(零开销) → SWC → Com Stack 编码 → SPI(32B 固定帧) → Platform ReadFrame → Service DiffCodec 解码 → Communication vsomeip notify


## 8. 项目目录结构

```
vehicle_gateway/
├── mcu/                          # AUTOSAR CP
│   ├── mcal/                     # GpioDrv, AdcDrv, PwmDrv, McuDrv
│   ├── signal_com/               # ComStack, CanDrv, SpiDrv, com_stub.h
│   ├── signal_app/               # VehicleSpeed_SWC, SteeringAngle_SWC, BodySignals_SWC
│   └── main.c
│
├── mpu/                          # AUTOSAR AP
│   ├── platform/                 # SpiDevice, ThreadPool, Logger
│   ├── svc/                      # SignalBridgeService, SignalBuffer, DiffCodec
│   ├── comm/                     # CommunicationService, Stubs
│   └── main.cpp
│
└── tools/dbc/
    └── vehicle_signals.dbc
```

---

> **v2.2** 极简开发参照版 | 保留所有编码时需要的设计结论，删除解释/代码/原理 | ~280 行
