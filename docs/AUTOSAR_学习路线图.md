# AUTOSAR 概念学习路线图

> **学习策略**: 不单独学 AUTOSAR，而是**随 CAN / SPI / SOME/IP 学习过程穿插理解**。
> 学一层通信，就附带上层 AUTOSAR 抽象概念，在实践中建立认知。

---

## 本工程 AUTOSAR CP/AP 分层总览

```
┌──────────────────────────────────────────────────────────────┐
│                    MCU 端 (AUTOSAR CP)                        │
│                                                               │
│  ⑤ App/Swc_SignalGateway     SWC 应用层                       │
│  ④ RTE/                      运行时环境 (宏 + volatile 共享内存) │
│  ③ EcuAbstraction/           ECU 抽象层 (CanIf, IoHwAb, SpiIf) │
│  ② CDD/Uart/                 复杂驱动                          │
│  ① MCAL/                     微控制器抽象层                    │
│     Gpio/ Mcu/ Can/ Spi/ Port/                           │
├──────────────────────────────────────────────────────────────┤
│                    SOC 端 (AUTOSAR AP)                        │
│                                                               │
│  ④ apps/DomainController     Application                     │
│  ③ communication/            ara::com + SpiGateway            │
│  ② platform/                 ara::core/exec/log + SpiDriver   │
│  ① diag/ara/diag/            UdsServer                        │
└──────────────────────────────────────────────────────────────┘
```

---

## 第一阶段：MCU CAN 通信基础（无 AUTOSAR）

> **你已有 Linux 开发基础，MCU 端 MCAL 层使用 NXP SDK DRV API 开发。**
> 此阶段只关注 CAN 通信必须的最小知识集：C 嵌入式基础 + 时钟系统 + FlexCAN + SDK 使用模式。

---

## 第二阶段：CAN 通信精进（附 AUTOSAR CP 概念）

### 2.1 MCAL Can 驱动 → SDK FlexCAN

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `Can_Init()` → `FLEXCAN_DRV_Init()` | **Can_Init** — MCAL Can 驱动 | SDK API 封装的 FlexCAN 初始化 |
| `Can_Transmit()` → `FLEXCAN_DRV_SendBlocking()` | **Can_Write** — MCAL Can 驱动 | 构造并发送 CAN 帧 |
| `Can_Receive()` → `FLEXCAN_DRV_ReceiveBlocking()` | **Can_Read** / **Can_RxIndication** | 接收中断/查询读取 |
| `mcu/MCAL/Can/config/Can_Cfg.h` | **CanHardwareObject** | 硬件对象对应消息缓冲区 |

**一句话**: AUTOSAR CP 的 MCAL Can 层就是对 FlexCAN SDK 驱动的标准化封装——`Can_Transmit()` 内部调 `FLEXCAN_DRV_SendBlocking()`。

### 2.2 CanIf 抽象层

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `CanIf_Transmit()` → `Can_Transmit()` | **CanIf_Transmit** | 统一 CAN 帧发送接口 |
| `CanIf_RxIndication()` 回调 | **CanIf_RxIndication** | 统一 CAN 帧接收回调 |
| `mcu/EcuAbstraction/CanIf/` | **CanIf** 层 | 屏蔽具体 CAN 控制器差异 |

**一句话**: CanIf 层让上层网络协议（如 DoIP、J1939）不关心底层是哪个 CAN 控制器。

### 2.3 SPI 通信 → Com Stack + Spi

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `SpiIf_WriteIb()` → `Spi_WriteIb()` | **Spi** (MCAL) | SPI 32B 固定帧收发 |
| `SpiIf_ReadIb()` → `Spi_ReadIb()` | **Spi** (MCAL) | SPI 帧接收 |
| 差分编码 / sensor_mask | **PduR** + **Com** | 信号↔PDU 编解码 |
| CRC8 校验 | **E2E_P01** (CP) | 数据完整性保护 |

**一句话**: MCU 端通过 SPI 发送差分帧给 SOC，同时 FlexCAN 发送 CAN 帧给其他域控制器——SPI 和 CAN 是 MCU 端两个并行的输出通道。

### 2.4 MCAL 其他模块

| 模块 | AUTOSAR CP 对应 | 核心 API |
|------|----------------|---------|
| `mcu/MCAL/Gpio/` | **Gpio** | `Gpio_ReadPin()`, `Gpio_WritePin()` |
| `mcu/MCAL/Mcu/` | **Mcu** | `Mcu_InitClock()`, `Mcu_GetCoreFreq()` |
| `mcu/MCAL/Port/` | **Port** | `Port_SetPinMode()`, `Port_SetMuxMode()` |
| `mcu/EcuAbstraction/IoHwAb/` | **IoHwAb** | `IoHwAb_ReadPin()` |

### 2.5 RTE — 运行时环境

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `Rte_Read(VehicleSpeed)` | **Rte_Read** | SWC 读取信号值 |
| `Rte_Write(DoorStatus, val)` | **Rte_Write** | SWC 写入信号值 |
| `volatile SharedSignalsType g_shared_signals` | **RTE 共享缓冲区** | ISR 写、SWC 读 |

**一句话**: RTE 是 SWC 之间的通信中枢。本工程用 `volatile` 共享内存 + 宏模拟，编译期展开，零开销。

---

## 第三阶段：SOME/IP 服务通信（附 AUTOSAR AP 概念）

### 3.1 SOME/IP 协议 → ara::com

| SOME/IP 概念 | AUTOSAR AP 概念 | 核心关系 |
|-------------|----------------|---------|
| Method (请求/响应) | **ara::com Method** | 客户端调用，返回结果 |
| Event (事件推送) | **ara::com Event** | 服务端主动通知客户端 |
| Field (属性) | **ara::com Field** | 可读可写的服务属性，带通知 |
| Service ID + Method ID | **ServiceInterface** | 唯一标识一个服务方法 |

**一句话**: ara::com 是 AUTOSAR AP 对 SOME/IP 的 C++ 封装——SOME/IP 是"网络协议"，ara::com 是"编程模型"。

### 3.2 SOM/IP 服务发现 → ara::sd

| SOM/IP SD 概念 | AUTOSAR AP 概念 | 核心关系 |
|----------------|----------------|---------|
| OfferService | **ara::sd::OfferService** | 服务端宣布自己启动 |
| FindService | **ara::sd::FindService** | 客户端发现可用服务 |
| SubscribeEventgroup | **ara::sd::Subscribe** | 客户端订阅事件组 |

**一句话**: ara::sd 让服务之间的发现和绑定零配置、动态化，是实现 SOA 的核心。

### 3.3 SOM/IP 序列化 → E2E (端到端保护)

| SOM/IP 序列化 | AUTOSAR AP E2E | 核心关系 |
|---------------|---------------|---------|
| 序列化规则 | **E2E_P01/P02** | 自定义数据序列化 + CRC |
| 反序列化 | **E2E_Check()** | 校验数据完整性和真实性 |

**一句话**: E2E 在 SOME/IP 序列化基础上增加了 CRC 和计数器，防止通信篡改和重放。

---

## 整体概念地图

```
  ┌──────────────────────────────────────────────────────────────┐
  │                     AUTOSAR AP (自适应平台)                    │
  │  ┌──────────┬───────────┬─────────────┬──────────────────┐  │
  │  │ ara::com │ ara::sd   │  ara::diag  │  ara::core       │  │
  │  │ 服务模型  │ 服务发现  │  诊断管理   │  ErrorCode, etc   │  │
  │  └────┬─────┴────┬──────┴──────┬──────┴──────┬───────────┘  │
  │       │          │             │             │              │
  │       └── SOME/IP 协议 ──── SOME/IP SD ─── UDS ──────────┘  │
  ├──────────────────────────────────────────────────────────────┤
  │                     AUTOSAR CP (经典平台)                      │
  │  ┌─────────┬────────┬────────┬─────────┬──────────────────┐  │
  │  │   RTE   │  Com   │ PduR   │  CanIf  │  Can             │  │
  │  │ SWC通信  │ 信号管理│PDU路由 │抽象接口 │ MCAL驱动          │  │
  │  └────┬────┴───┬────┴───┬────┴────┬────┴────┬─────────────┘  │
  │       │        │        │         │         │               │
  │       │        └────────┴─── CAN 帧 ────────┘               │
  │       │                                                      │
  │       └── volatile 共享内存 (RTE 模拟)                        │
  ├──────────────────────────────────────────────────────────────┤
  │                    硬件层 (当前动手区)                        │
  │   S32K144 FlexCAN ─── USB-CAN 工具 ─── SocketCAN (Ubuntu)   │
  │   (物理 MCU)          (USB 桥接)        (虚拟机)              │
  │                                                               │
  │   S32K144 LPSPI ──── FT2232H (SPI Slave) ── Ubuntu (libmpsse)│
  │   (SPI Master)       (USB-SPI 桥)          (虚拟机)           │
  │                                                               │
  │   S32K144 LPUART ─── USB-UART ─── 宿主机串口 (调试日志)       │
  └──────────────────────────────────────────────────────────────┘
```

---

## 学习建议

1. **不跳步**: 学 CAN 时不要急着看 ara::com，理解 CAN 帧格式后，CP 的概念自然能理解
2. **每层都动手**: MCAL 驱动 → ECU 抽象层 → RTE → SWC → SOME/IP，每层都写代码
3. **回头看**: 学完一层后，回头看 AUTOSAR 概念对照表，你会突然明白那些抽象概念的意义
4. **概念到概念**: AUTOSAR 概念看一两遍记不住很正常，每次学新内容时温习旧概念，逐渐内化
