# AUTOSAR CP CAN 通信栈

> 以本项目的 MCU 端代码为例，逐层讲解 AUTOSAR Classic Platform 的 CAN 通信栈
> 架构、各层职责、API 调用链、数据收发路径，以及当前实现状态。

---

## 阅读前你需要知道

本文是五篇 CAN AUTOSAR 系列的总纲。假设你：

- 知道 CAN 总线的基本概念（差分线、帧结构、ID + DLC + 最多 8 字节数据）
- 有 C 语言基础（结构体、指针、函数指针）
- 如果用 S32K144 开发板实操，需要知道如何编译和烧录

如果对 CAN 协议本身不熟，先看 [CAN 原理系列](../CAN原理/)。

本文用到的缩写会在正文**首次出现时**解释，也可以在下面快速查阅。


## 下一步读什么

读完本文后，按以下顺序继续：

| 序号 | 文档 | 内容 |
|------|------|------|
| 2 | [MCAL Can 驱动详解](./2_MCAL_Can驱动详解.md) | 最底层——硬件怎么收发帧 |
| 3 | [L-PDU 详解](./3_L-PDU数据链路层协议数据单元详解.md) | CAN 硬件帧——`Can_PduType` 定长 8 字节 |
| 4 | [CanIf CAN 接口层详解](./4_CanIf_CAN接口层详解.md) | PDU ID 抽象——上层不再关心 CAN ID |
| 5 | [N-PDU 详解](./5_N-PDU网络层协议数据单元详解.md) | I-PDU/N-PDU 的区别、PCI 编解码 |
| 6 | [CanTp CAN 传输层详解](./6_CanTp_CAN传输层详解.md) | 传输协议——大块数据的分段、重组、流控 |
| 7 | [PduR 路由层详解](./7_PduR_PDU路由层详解.md) | PDU 路由器——Com↔CanTp 转发桥梁 |
| 8 | [I-PDU 详解](./8_I-PDU交互层协议数据单元详解.md) | 信号容器——三种 PDU 中最"高级"的一种 |
| 9 | [Com 通信层详解](./9_Com_通信层详解.md) | 顶层——信号打包、Update Bit、超时检测 |

---

## 1. 整体架构：分层模型

AUTOSAR CP 将 CAN 通信拆分为多个层次，从应用信号到物理总线逐层向下：

> **术语速查**：本文用到的缩写及其全称。
> - **SWC** (Software Component) — 应用软件组件，写业务逻辑的地方
> - **RTE** (Runtime Environment) — 运行时环境，SWC 与 BSW 之间的胶水层
> - **Com** (Communication) — 通信服务层，信号 ↔ PDU 的打包/解包
> - **PduR** (PDU Router) — PDU 路由器，在模块间转发 PDU
> - **PDU** (Protocol Data Unit) — 协议数据单元，AUTOSAR 栈中各层传递的数据包
> - **I-PDU** (Interaction Layer PDU) — Com 层传递的 PDU，含多个信号
> - **CanTp** (CAN Transport Protocol) — CAN 传输层，ISO 15765-2 多帧分包/重组
> - **CanIf** (CAN Interface) — CAN 接口层，PDU ↔ CAN 帧的硬件无关抽象
> - **MCAL** (Microcontroller Abstraction Layer) — 微控制器抽象层，直接访问硬件寄存器
> - **HTH** (Hardware Transmit Handle) — 硬件发送句柄，bit[15:8]=Controller, bit[7:0]=MB Index (AUTOSAR SWS_Can_00009)
> - **HRH** (Hardware Receive Handle) — 硬件接收句柄，即 RX Mailbox 索引
> - **MB** (Message Buffer / Mailbox) — FlexCAN 硬件中的消息缓冲单元
> - **DET** (Development Error Tracer) — 开发错误追踪，AUTOSAR 标准错误报告机制

```
 ┌──────────────────────────────────────────────────────────────────────┐
 │                      SWC (应用软件组件)                               │
 │                   mcu/App/Swc_SignalGateway/                          │
 ├──────────────────────────────────────────────────────────────────────┤
 │                                                                      │
 │  ┌─────────────────────────────────────────────────────────────┐    │
 │  │                     RTE (运行时环境)                          │    │
 │  │               mcu/RTE/Rte.h                                  │    │
 │  │  · SWC 与 BSW 之间的胶水层                                   │    │
 │  │  · Rte_Send_xxx() / Rte_Receive_xxx() → 映射到 Com API      │    │
 │  │  · 信号 → I-PDU 映射 (Rte_SignalToPduMapping)               │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ RTE → Com                                  │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    Com (通信服务)                             │    │
 │  │          mcu/Services/Com/include/Com.h                       │    │
 │  │  · 信号 ↔ PDU 打包/解包                                       │    │
 │  │  · Com_SendSignal() / Com_ReceiveSignal()                    │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ Com → PduR                                │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    PduR (PDU 路由器)                          │    │
 │  │          mcu/Services/PduR/include/PduR.h                     │    │
 │  │  · I-PDU 路由：Com ↔ CanIf（也支持 Com ↔ SpiIf 等）          │    │
 │  │  · PduR_ComTransmit() / PduR_CanIfRxIndication()            │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ PduR → CanTp → CanIf                       │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    CanTp (传输层)                             │    │
 │  │          mcu/Services/CanTp/include/CanTp.h                   │    │
 │  │  · ISO 15765-2 多帧分包/重组 (N-PDU)                        │    │
 │  │  · CanTp_Transmit() → SF / FF+CF+FC                          │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ CanTp → CanIf (PduInfoType = N-PDU)       │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    CanIf (CAN 接口)                           │    │
 │  │          mcu/EcuAbstraction/CanIf/include/CanIf.h             │    │
 │  │  · 硬件无关的 CAN 收发抽象                                   │    │
 │  │  · N-PDU → L-PDU 格式转换                                    │    │
 │  │  · CanIf_Transmit() / CanIf_RxIndication()                   │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ CanIf → Can (MCAL)                         │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    Can (MCAL 驱动)                            │    │
 │  │              mcu/MCAL/Can/include/Can.h                       │    │
 │  │  · 直接硬件访问 (CAN PAL → FlexCAN 寄存器)                    │    │
 │  │  · Can_Write(Hth, PduInfo) → 物理 CAN 帧                    │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │                                            │
 └─────────────────────────┼────────────────────────────────────────────┘
                           │
              ┌────────────▼────────────┐
              │       CAN 收发器         │
              │   (TJA1050 / TJA1040)    │
              └────────────┬────────────┘
                           │
                    CAN_H  │  CAN_L
                    ───────┴───────
                       CAN 总线
```

---

## 2. 各层详细说明

### 2.1 Can — MCAL 驱动层（已实现 ✅）

**职责**：直接访问 CAN 硬件（FlexCAN 控制器），发送/接收物理 CAN 帧。

> **SWS 规范**：`Can_Init` (SWS_Can_00013, 支持多路 CAN 独立初始化)、`Can_Write` (SWS_Can_00106, HTH 编码 Controller+MB)、`Can_SetControllerMode` (SWS_Can_00098)、`Can_DeInit` (SWS_Can_00014)、`Can_MainFunctionWrite` (SWS_Can_00047)、`Can_MainFunctionRx` (SWS_Can_00048)。
>
> **HTH 编码** (SWS_Can_00009)：`bit[15:8]=Controller  bit[7:0]=MB_Index`，`Can_Write(Hth, PduInfo)` 不传 Controller 参数——Controller 从 HTH 高位解码。宏：`CAN_HTH_MAKE(ctrl, mb)`, `CAN_HTH_CTRL(hth)`, `CAN_HTH_MB(hth)`。
>
> **多路 CAN 架构**：每 Controller 独立 `Can_CtrlState`（实例、状态、buffer），`Can_MainFunctionRx/Write` 遍历所有已初始化控制器。

**文件位置**：
- `mcu/MCAL/Can/include/Can.h` — 类型定义和 API 声明
- `mcu/MCAL/Can/src/Can.c` — 驱动实现（封装 NXP CAN PAL）

**核心类型**：

```c
// Can.h — CAN L-PDU 数据单元
typedef struct {
    Can_IdType   id;          // CAN ID (11-bit 或 29-bit)
    uint8_t      length;      // 数据长度 (0~8)
    bool         is_extended; // 是否扩展帧
    bool         is_remote;   // 是否远程帧
    uint8_t      data[8];     // 数据载荷 — 定长 8 字节
} Can_PduType;

// Can.h — AUTOSAR 标准类型
typedef uint32_t Can_IdType;
typedef uint16_t Can_HwHandleType;
typedef enum { CAN_ERRORSTATE_ACTIVE, CAN_ERRORSTATE_PASSIVE,
               CAN_ERRORSTATE_BUSOFF } Can_ErrorStateType;
```

**核心 API**：

```c
// Can.h — AUTOSAR 标准 API
Std_ReturnType Can_Init(const Can_ConfigType *ConfigPtr);
Std_ReturnType Can_DeInit(void);
Std_ReturnType Can_SetControllerMode(Can_ControllerType Controller,
                                     Can_ControllerStateType Transition);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
status_t       Can_Read(uint8_t Controller, uint8_t Hrh, Can_PduType *PduInfo);
Std_ReturnType Can_GetControllerErrorState(Can_ControllerType, Can_ErrorStateType*);
Std_ReturnType Can_GetControllerMode(Can_ControllerType, Can_ControllerStateType*);
```

> `Can_Write` 按 AUTOSAR 标准只传 Hth（不含 Controller），Controller 由驱动内部维护。RX 已从轮询升级为**中断驱动**模式，不再直接调用 `Can_Read`。

**实现要点**：

- **Can_Write**：每次发送前调 `CAN_ConfigTxBuff()` 重配 TX MB（上次发送后 MB 状态非空闲）
- **Can_Read**：由 `Can_MainFunctionRx()` 内部调用，ISR 标记 → 主循环消费
- **Can_EnableInterrupts**：`CAN_InstallEventCallback(Can_IrqCallback)` + 逐一 `CAN_Receive()` 武装 RX MB
- **Can_MainFunctionRx**：检查 ISR 标记 → 读静态 buffer → `CanIf_RxIndication` → 重新武装 MB

**与 NXP SDK 的关系**：

```
    AUTOSAR MCAL 层               NXP SDK 层                   硬件
   ┌─────────────────┐     ┌──────────────────────┐     ┌──────────┐
   │ Can_Init()      │────→│ CAN_Init()            │────→│          │
   │ Can_Write()     │────→│ CAN_ConfigTxBuff()    │────→│ FlexCAN  │
   │                 │     │ CAN_Send()            │     │ 寄存器   │
   │ Can_Read()      │────→│ CAN_Receive()         │────→│          │
   │                 │     │ (can_pal.h)            │     │          │
   └─────────────────┘     └──────────────────────┘     └──────────┘
```

MCAL 封装的好处：
- 上层代码不依赖任何 NXP SDK 头文件（`Can.h` 不 `#include` 任何 SDK 头）
- 更换 MCU 平台时只需修改 `Can.c` 的内部实现，上层代码无需改动
- 遵守 AUTOSAR 规范命名（`Can_Init`, `Can_Write`, `Can_Read` 等）

---

### 2.2 CanIf — CAN Interface 层（已实现 ✅）

**职责**：在 MCAL Can 驱动之上提供**硬件无关**的 CAN 通信接口。
对上（PduR/CanTp）隐藏具体硬件细节，对下（Can）统一调用 MCAL 接口。

> **SWS 规范**：`CanIf_Transmit` (SWS_CanIf_00050)、`CanIf_RxIndication` (SWS_CanIf_00030)、`CanIf_TxConfirmation` (SWS_CanIf_00040)、DET ModuleId = 0x32。

**文件位置**：
- `mcu/EcuAbstraction/CanIf/include/CanIf.h` — 类型定义和 API 声明
- `mcu/EcuAbstraction/CanIf/src/CanIf.c` — 完整实现（~210 行）
- `mcu/EcuAbstraction/CanIf/config/CanIf_Cfg.c` — PDU 配置表（由 YAML 自动生成）

**核心 API**：

```c
// CanIf.h
void    CanIf_Init(void);
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
void           CanIf_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void           CanIf_TxConfirmation(PduIdType TxPduId);
CanIf_PduIdType CanIf_FindPduIdByCanId(uint32_t CanId);
```

**当前状态**：已实现（详见 [4_CanIf_CAN接口层详解](./4_CanIf_CAN接口层详解.md)）。

已实现功能：
- `CanIf_Transmit` — PDU 查表 → 格式转换 → `Can_Write`（完整实现）
- `CanIf_RxIndication` — PDU ID 校验 → 转发 `PduR_CanIfRxIndication`（完整实现）
- `CanIf_TxConfirmation` — 发送完成确认 → 转发 `PduR_CanIfTxConfirmation`（完整实现）
- **TX 完成回调**：`CanIf_McalTxCallback` 注册到 MCAL，发送完成时按 HTH 反查 PDU ID（完整实现）
- DET 错误检测框架（ModuleId=0x32, 3 种 ErrorId）
- `CanIf_FindPduIdByCanId` — CAN ID → PDU ID 反查（RX 路径辅助）
- 配置表由 `signals.yaml` 自动生成（`CanIf_Cfg.c` + `CanIf_PduId.h`）

---

### 2.3 CanTp — CAN Transport Layer（已实现）

**职责**：实现 ISO 15765-2 多帧传输协议。当 PDU 超过 8 字节时进行分包/重组。

**文件位置**：`mcu/Services/CanTp/include/CanTp.h`

**核心概念**：

```c
// CanTp.h — ISO 15765-2 四种帧类型
typedef enum {
    CANTP_SF = 0,   // Single Frame: 单帧（数据 ≤ 7 字节，一个 CAN 帧搞定）
    CANTP_FF = 1,   // First Frame:  首帧（多帧传输的第一帧，含总长度）
    CANTP_CF = 2,   // Consecutive Frame: 续帧（FF 后面的数据帧）
    CANTP_FC = 3,   // Flow Control: 流控帧（接收方控制发送方速率）
} CanTp_FrameType;
```

**多帧传输流程**：

```
    发送方 (Sender)                          接收方 (Receiver)
    ──────                                   ──────
    FF (First Frame) ───────────────────→
    (含总长度, 前 6 字节数据)
                                            ←─────────────────── FC (Flow Control)
                                               (BS=块大小, STmin=最小间隔)
    CF (Consecutive #1) ───────────────→
    (下 7 字节数据)
    CF (Consecutive #2) ───────────────→
    (再下 7 字节数据)
    ...
    CF (Consecutive #N) ───────────────→
    (最后数据)
```

**当前状态**：已激活，由 EcuM 编译和初始化。PCI 编解码 + SF/FF/CF/FC 状态机完整实现（TX 发 FF 后等待 FC 流控，RX 收 FF 后自动回 FC）。另实现：
- **TX 确认链**：SF 发送完成经 CanIf→PduR 回到 `CanTp_TxConfirmation`，支持 N_As 超时（详见 [6_CanTp_CAN传输层详解](./6_CanTp_CAN传输层详解.md)）
- 超时保护：N_As（SF 确认）/ N_Bs（等 FC）/ N_Cr（等 CF）

---

### 2.4 PduR — PDU Router（已实现 ✅）

**职责**：在通信协议栈中**路由 I-PDU**。Com 和 CanTp/CanIf 之间的所有 PDU
都经过 PduR 转发。PduR 不处理数据内容——只做"接线板"式的调用链转发。

**文件位置**：`mcu/Services/PduR/include/PduR.h`

**路由关系**：

```
                        PduR
                        ┌───┐
          Com ─────────→│   │─────────→ CanTp (→ CanIf → Can)
                        │   │
          CanTp ───────→│   │─────────→ Com
                        └───┘
```

**核心 API**：

```c
// PduR.h
void    PduR_Init(void);
Std_ReturnType PduR_ComTransmit(PduIdType PduId, const PduInfoType *PduInfoPtr);
void           PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void           PduR_CanIfTxConfirmation(PduIdType TxPduId);   // N-PDU 发送确认
void           PduR_CanTpTxConfirmation(PduIdType TxPduId);   // I-PDU 发送确认
```

**当前状态**：已实现 ✅。Com↔CanTp 路由全部打通。RX 路径（CanIf→CanTp→PduR→Com）和 TX 路径（Com→PduR→CanTp→CanIf）完整工作，CAN 收发实测通过。详见 [7_PduR_PDU路由层详解](./7_PduR_PDU路由层详解.md)。

---

### 2.5 Com — Communication Service（已实现 ✅）

**职责**：AUTOSAR 通信栈的最顶层。负责信号的打包/解包、发送模式控制、Update Bit 管理、信号超时检测（Deadline Monitoring）以及 DET 错误报告。

**文件位置**：
- `mcu/Services/Com/include/Com.h` — API 声明 + 类型定义
- `mcu/Services/Com/src/Com.c` — 完整实现（~950 行）
- `mcu/Services/Com/config/Com_Cfg.h` — 信号与 I-PDU 配置表
- `mcu/Services/Com/config/Com_Cfg.c` — 配置实例

**核心概念——你需要先理解的三个东西**：

```
                 信号 (Signal)                     I-PDU
              ┌──────────────────┐           ┌──────────────┐
              │ TestTxCounter    │──┐        │              │
              │ (32-bit, byte0-3)│  │   ┌───→│  CAN ID 0x123│
              └──────────────────┘  │   │    │  (TX, 500ms) │
                                    ├───┤    │              │
              ┌──────────────────┐  │   │    └──────────────┘
              │ TestTxMagic0     │──┤   │
              │ (8-bit, byte4)   │  │   │
              └──────────────────┘  │   │    一个 I-PDU = 一条 CAN 帧
                                    │   │    多个信号共享一个 I-PDU
              ┌──────────────────┐  │   │    就像多个包裹装进一个集装箱
              │ TestTxMagic1     │──┘   │
              │ (8-bit, byte5)   │      │    ┌──────────────┐
              └──────────────────┘      └───→│  CAN ID 0x100│
                                             │  (RX, 事件)   │
              ┌──────────────────┐      ┌───→│              │
              │ TestRxData       │──────┘    └──────────────┘
              │ (64-bit, byte0-7)│
              └──────────────────┘
```

1. **信号 (Signal)**：SWC 操作的最小数据单元。比如"车速"是一个 16-bit 整数，"踏板位置"是一个 8-bit 整数。
2. **I-PDU (Interaction Layer PDU)**：一组信号的集合，映射到一条 CAN 帧。多个信号**共享**同一个 CAN 帧的数据区。
3. **Shadow Buffer**：信号的"影子副本"。Com_SendSignal 不直接发 CAN 帧，而是先把信号值写入 Shadow Buffer + 打包到 I-PDU 字节数组。真正的发送由 Com_MainFunction 调度。

**核心 API**：

```c
// Com.h — 完整实现
void Com_Init(void);
void Com_MainFunction(void);

// 信号收发
void Com_SendSignal(Com_SignalIdType SignalId, const void *SignalData);
void Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalData);

// ★ Update Bit 模式: SWC 轮询更新标志位，不直接比较值
uint8_t Com_GetUpdateBit(Com_SignalIdType SignalId);

// ★ Deadline Monitoring: 检查信号是否超时
Com_SignalStatusType Com_GetSignalStatus(Com_SignalIdType SignalId);

// IPduGroup 通信模式控制
void Com_IPduGroupStart(Com_IPduIdType GroupId);
void Com_IPduGroupStop(Com_IPduIdType GroupId);

// 回调接口 (被 PduR 调用)
void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void Com_TxConfirmation(PduIdType TxPduId);
```

**关键机理解释**：

**1. Transfer Property（发送触发模式）——信号更新后"什么时候发"？**

| 模式 | 行为 | 类比 |
|------|------|------|
| TRIGGERED | 信号值改变 → **立即**调用 PduR_ComTransmit | 急件——写了马上发 |
| PENDING | 信号值改变 → 标记 dirty → **等** Com_MainFunction 统一发 | 普通——攒一批一起发 |
| NONE | 信号改变不触发发送，仅按周期时间发 | 定时报告——到点才发 |

一个 I-PDU 里可以混用三种模式。比如我们 TX 的三个信号：
- TestTxCounter → TRIGGERED（计数器变了马上发）
- TestTxMagic0 → PENDING（标记一下，等 MainFunction 统一发）
- TestTxMagic1 → NONE（只管写，不管发，等 500ms 周期）

**2. COM 层不直接发送！Shadow Buffer 模式**

```
SWC 调用 Com_SendSignal(COUNTER, &value)
  │
  ├─→ 写入 Shadow Buffer (记录信号值，供 Com_ReceiveSignal 读取)
  ├─→ 调用 Com_PackSignal() 把 value 按 bit_position/bit_size 打包到 I-PDU 字节数组
  └─→ 标记 I-PDU dirty

... 一段时间后 ...

Com_MainFunction() 被 EcuM 周期调用
  ├─→ 检查哪些 I-PDU dirty
  ├─→ 检查周期时间是否到 (cycle_time_ms)
  └─→ 调用 PduR_ComTransmit() → 真正发出 CAN 帧
```

为什么不直接发？两个原因：
- **批处理**：多个 SWC 写同一 I-PDU 的不同信号，只发一次 CAN 帧
- **时序解耦**：SWC 写信号的时机 ≠ CAN 总线发送的时机

**3. Update Bit（更新标志位）——AUTOSAR 最经典的模式**

```
SWC 处理逻辑:
  if (Com_GetUpdateBit(TEST_RX_DATA)) {   // 有新数据吗？
      读信号值 → 处理                       // Update Bit 自动清 0
  }
```

为什么不用"比较新旧值"？
- 值可能先变 1→2→1，比较发现"没变"，但你漏掉了中间的变化
- Update Bit 是"发生过更新事件"的标志，不依赖值本身

**4. Deadline Monitoring（信号超时检测）**

Com_MainFunction 每秒检查：RX 信号距上次收到新数据是否超过 `timeout_ms`？超过就标记 COM_SIGNAL_TIMEOUT。这是 AUTOSAR 安全监控的基础——"数据不能太老"。

**5. DET 错误报告**

非法参数（如 Com_SendSignal(999)、传 NULL 指针）会触发 `Det_ReportError()`，输出到 UART 日志。这在开发期帮你快速定位 bug。

**当前状态**：完整实现 ✅。模块由 EcuM 编译和初始化，Com_MainFunction 周期调度。CAN 收发实测通过（candump + cansend 验证）。详见 [9_Com_通信层详解](./9_Com_通信层详解.md)。

---

## 3. 数据收发完整路径

### 3.1 发送路径（TX，自上而下）

```
SWC/App
  │
  │ Rte_Send_SignalX(&value)     ← SWC 调 RTE API
  ▼
RTE: 信号 → I-PDU 映射
  │ 根据 Rte_SignalToPduMapping 找到目标 I-PDU
  │
  │ Com_SendSignal(SignalId, &value)
  ▼
Com: 信号 → PDU 打包
  │ 将信号值按位布局写入 I-PDU 缓冲区
  │ 根据传输模式决定是否触发发送
  │
  │ PduR_ComTransmit(PduId, &PduInfoPtr)
  ▼
PduR: 查找路由表
  │ 根据 PduId 决定目标: CanTp (CAN 路径) 或 SpiIf (SPI 路径)
  │
  │ CanTp_Transmit(PduId, &PduInfoPtr)
  ▼
CanTp: 多帧分包 (ISO 15765-2)
  │ 若 data_len ≤ 7: 直接发 Single Frame (N-PDU)
  │ 若 data_len > 7: 分为 FF + 多个 CF, 等待 FC 流控
  │
  │ CanIf_Transmit(PduId, &PduInfoPtr)  ← N-PDU
  ▼
CanIf: N-PDU → L-PDU 映射
  │ 根据 PDU ID 查找 CAN ID 和 Hth
  │
  │ Can_Write(Hth, &CanPdu)      ← L-PDU
  ▼
Can: MCAL 驱动
  │ CAN_ConfigTxBuff() — 每次发送前重配 TX MB
  │ CAN_Send() — 通过 PAL 调用 FLEXCAN_DRV_Send
  │
  ▼
FlexCAN 硬件: 产生物理 CAN 帧 → CAN 收发器 → CAN_H/CAN_L 总线
```

### 3.2 接收路径（RX，自下而上）

```
CAN_H/CAN_L 总线
  │
  ▼
FlexCAN 硬件: 接收到匹配 ID 的帧 → 存入对应 RX Mailbox
  │
  │ Can_Read(Controller, Hrh, &CanPdu)
  ▼
Can: MCAL 驱动
  │ CAN_Receive() — 通过 PAL 读取 MB 数据
  │ 填充 Can_PduType (L-PDU)
  │
  │ CanIf_RxIndication(RxPduId, &PduInfoPtr)  ← N-PDU
  ▼
CanIf: L-PDU → N-PDU 映射
  │ 根据 CAN ID 查找 PDU ID
  │
  │ PduR_CanIfRxIndication(RxPduId, &PduInfoPtr)
  ▼
PduR: 查找路由表
  │ 根据 RxPduId 决定目标: Com (数据) 或 CanTp (需要重组的多帧)
  │
  │ ┌─ 单帧路径 ─→ Com_RxIndication(PduId, &PduInfoPtr)
  │ └─ 多帧路径 ─→ CanTp_RxIndication(PduId, &PduInfoPtr)
  ▼
CanTp (多帧): 重组 CF 帧 → 完整 I-PDU → PduR → Com

Com: PDU → 信号解包
  │ 从 I-PDU 缓冲区中按位布局提取各 Signal 值
  │
  │ Com_ReceiveSignal(SignalId, &value) (RTE 调用)
  ▼
RTE: I-PDU → 信号映射
  │ 根据 Rte_PduToSignalMapping 提取各信号值
  │
  │ Rte_Receive_SignalX(&value)     ← SWC 调 RTE API
  ▼
SWC/App
```

---

## 4. 当前项目实现状态

### 4.1 状态总览

```
    RTE:    [已实现]    SWC↔Com 信号映射，Rte_Write→Com_SendSignal / Rte_Read→Com_ReceiveSignal
    Com:    [完整实现 ✅] 信号打包/解包 + Update Bit + Deadline Monitoring + DET + IPduGroup
    Det:    [已实现]    开发错误追踪，Det_ReportError → UART 日志
    PduR:   [已实现]    Com↔CanTp 路由，RX/TX 回调链完整打通
    CanTp:  [已实现]    SF/FF/CF/FC 状态机 + 超时保护
    EcuM:   [已实现]    按 AUTOSAR 顺序调度 BSW 模块初始化和 MainFunction
    CanIf:  [已实现]    PDU 抽象(AUTOSAR PduInfoType) + DET + YAML 自动配置
    Can:    [已实现]    AUTOSAR 标准 MCAL 驱动(AUTOSAR Can_Write 签名)，详见 2
```

### 4.2 当前实际使用的数据路径

> **RX 模式**：已经从轮询升级为**中断驱动**（`Can_EnableInterrupts` → ISR 标记 → `Can_MainFunctionRx` 在主循环消费）。不再直接调用 `Can_Read()`。详见 [2_MCAL_Can驱动详解 section 4](./2_MCAL_Can驱动详解.md)。

`main.c` 的核心循环（简化版，完整代码见 `main.c`）：

```c
// BSW 初始化: EcuM_Init() → Can → CanIf → PduR → CanTp
// CAN RX: 中断驱动 (Can_EnableInterrupts + Can_MainFunctionRx)

for (;;) {
    // BSW 周期处理: 驱动 CanTp 流控状态机
    EcuM_MainFunction();

    // TX: CanTp_Transmit → SF(≤7B) 或 MF(>7B, FF→FC→CF)
    PduInfoType txPdu = { .SduId = 0U, .SduLength = dataLen, .SduDataPtr = txData };
    CanTp_Transmit(0U, &txPdu);

    // RX: ISR 标记 → Can_MainFunctionRx → CanIf → PduR → CanTp (重组)
    if (Can_MainFunctionRx()) {
        // CAN 帧已通过 CanIf → PduR → CanTp 进入重组状态机
    }
}
```

### 4.3 向完整 AUTOSAR 栈迁移的路线图

1. ✅ **CanIf → Can 桥接**：`CanIf_Transmit(PduId, PduInfoType*)` → `Can_Write(Hth, Can_PduType*)`

2. ✅ **ComStack_Types.h**：全栈统一 `PduInfoType`

3. ✅ **CanTp N-PDU 处理**：PCI 编解码 + SF/FF/CF/FC 状态机

4. ✅ **PduR + CanTp 集成**：EcuM 统一调度初始化 + RX/TX 回调链

5. ✅ **RTE 信号映射**：`Rte_Write→Com_SendSignal`, `Rte_Read→Com_ReceiveSignal`

6. ✅ **PduR 路由逻辑**：`PduR_ComTransmit→CanTp`, `PduR_CanTpRxIndication→Com_RxIndication`

7. ✅ **Com 信号打包**：Intel/Motorola 字节序 bit 级编解码 + Transfer Property

8. ✅ **中断模式**：RX 中断驱动（`Can_EnableInterrupts` + `Can_MainFunctionRx`）

9. ✅ **Com Update Bit**：每信号更新标志位 + `Com_GetUpdateBit` API

10. ✅ **Com Deadline Monitoring**：信号超时检测 + `Com_GetSignalStatus` API

11. ✅ **DET 错误报告**：`Det_ReportError(ModuleId, InstanceId, ApiId, ErrorId)`

12. ✅ **Link 顺序修复**：`$(OBJS)` 置于 `-lc` 之前，`memcpy`/`memset` 正常链接

13. ⬜ **CanTp FC 流控完善**：TX 侧完整的 FC 等待 + BS/STmin 速率控制

---

## 5. 关键设计要点

### 5.1 初学者最容易困惑的三个概念

**Q1: "信号" 和 "I-PDU" 到底什么关系？**

把 I-PDU 想象成一个**集装箱**，信号就是里面的**包裹**。一个集装箱里放多个包裹，共享一次运输（一条 CAN 帧）。每个包裹在集装箱里有自己的位置（bit_position）和大小（bit_size）。

**Q2: Com_SendSignal 为什么不立刻发？**

因为还有别的信号也在同一个 I-PDU 里。如果每个信号写的时候都立刻发，那一个 I-PDU 里 3 个信号就要发 3 条 CAN 帧——浪费。COM 层的策略是"**先写好，到时间（或凑齐一批）再发**"。

**Q3: AUTOSAR 里没有 `malloc` 和线程？**

对。AUTOSAR CP 里的所有东西——配置表、缓冲区、状态机——都是**编译时就确定**的。没有动态分配，没有抢占式线程。所有"并发"都是 `MainFunction` 周期调用 + 状态机驱动。

### 5.2 Update Bit 模式——SWC 怎么知道"有新数据到了"？

这是 AUTOSAR 最经典、最常用的数据新鲜度检查模式：

```c
// SWC 主循环
void Swc_MainFunction(void) {
    // 不比较信号值！用标志位！
    if (Com_GetUpdateBit(VEHICLE_SPEED)) {
        uint16_t speed;
        Com_ReceiveSignal(VEHICLE_SPEED, &speed);
        // 处理 speed...
        // Update Bit 已在 GetUpdateBit 中自动清 0
    }
    // 没新数据，直接跳过，继续下一轮
}
```

**为什么要这样？** 因为信号值可能"闪变"（1→0→1），比较新旧值会漏掉。Update Bit 记录的是"**发生过更新事件**"，不关心值本身。

### 5.3 Deadline Monitoring——数据不能太老

对于安全关键信号（如刹车位置），"3 秒没收到数据"比"数据值是 0"更可怕——前者意味着通信断了。

```c
// Com_MainFunction 中检测:
for each RX signal:
    if (current_time - last_update_time > timeout_ms):
        mark as COM_SIGNAL_TIMEOUT
    else:
        mark as COM_SIGNAL_OK
```

SWC 在读信号前先检查状态：
```c
if (Com_GetSignalStatus(BRAKE_POS) == COM_SIGNAL_TIMEOUT) {
    // 刹车信号超时！进入安全状态
}
```

### 5.4 DET 错误报告——开发期的"安全带"

AUTOSAR 每个 BSW 模块都配有 DET（Development Error Tracer）。当 API 被非法调用时，DET 记录错误但不中止程序。这相当于给你的代码上了个"**参数校验安全带**"：

```c
void Com_SendSignal(SignalId, data) {
    if (SignalId >= COM_SIGNAL_COUNT) {
        Det_ReportError(COM_MODULE_ID, 0, API_ID_SEND, COM_E_PARAM);
        return;  // 记录错误，安全返回，不崩
    }
    // ... 正常路径
}
```

### 5.5 模块 ID 与错误检测

项目中遵循 AUTOSAR DET（Development Error Tracer）规范，每层分配唯一的模块 ID：

| 模块 | Module ID | 文件 |
|------|-----------|------|
| Com | 22 (0x16) | `Com.c` |
| PduR | 21 (0x15) | `PduR.h` |
| CanTp | 28 (0x1C) | `CanTp.h` |
| CanIf | 50 (0x32) | `CanIf.c` |

### 5.6 每次 Can_Write 前必须重配 TX MB

这是该项目最重要的一个工程技巧：

```c
// 关键: 每次发送前必须调用 CAN_ConfigTxBuff 重新配置 TX MB,
// 否则 PAL 内部 FLEXCAN_DRV_Send 的 MB 状态检查返回 BUSY。
CAN_ConfigTxBuff(&Can_Instance, Hth, &Can_TxBuffCfg);
```

原因：NXP CAN PAL 的 `CAN_Send()` 内部会检查 MB 状态码（CODE 字段），
上一次发送完成后 MB 处于 INACTIVE 状态而非 READY 状态，不重新配置则
PAL 认为 MB 仍被占用，返回 `STATUS_BUSY`。

---

## 6. 架构关系图

```
┌───────────────────────────────────────────────────────────────┐
│                      Application Layer                        │
│            mcu/App/Swc_SignalGateway/src/main.c               │
│                                                               │
│  [当前] CanIf_Transmit() + Can_EnableInterrupts/Rx 中断       │
│  [未来] 通过 Rte_Send_xxx() / Rte_Receive_xxx()              │
└──────────────────────────┬────────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │                  RTE Layer                               │
    │                                                         │
    │  RTE (mcu/RTE)                                          │
    │  ├── 类型: Rte_SignalToPduMapping (信号↔I-PDU 映射)     │
    │  ├── API:  Rte_Send_xxx(), Rte_Receive_xxx()    [骨架]  │
    │  └── 职责: SWC↔BSW 胶水层, 信号路由                    │
    └──────────────────────┬──────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │                Services Layer                           │
    │                                                         │
    │  Com (mcu/Services/Com)                                 │
    │  ├── 类型: Com_SignalIdType, Com_SignalType             │
    │  └── API:  Com_SendSignal(), Com_ReceiveSignal()  [骨架] │
    │                                                         │
    │  PduR (mcu/Services/PduR)                               │
    │  ├── 类型: PduInfoType (ComStack_Types.h)               │
    │  └── API:  PduR_ComTransmit(), PduR_CanIfRxIndication() │
    │            [已激活]                                      │
    │                                                         │
    │  CanTp (mcu/Services/CanTp)                             │
    │  ├── 类型: CanTp_FrameType (SF/FF/CF/FC)                │
    │  └── API:  CanTp_Transmit(), CanTp_RxIndication() [已激活] │
    └──────────────────────┬──────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │            ECU Abstraction Layer                        │
    │                                                         │
    │  CanIf (mcu/EcuAbstraction/CanIf)                       │
    │  ├── 类型: PduInfoType (ComStack_Types.h)               │
    │  ├── API:  CanIf_Transmit(), CanIf_RxIndication()       │
    │  └── DET:  ModuleId=0x32, ApiId, ErrorId         [已实现] │
    └──────────────────────┬──────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │                MCAL Layer                               │
    │                                                         │
    │  Can (mcu/MCAL/Can)                                     │
    │  ├── 类型: Can_IdType, Can_HwHandleType, Can_PduType    │
    │  ├── API:  Can_Init(), Can_Write(), Can_Read()  [已实现] │
    │  ├── API:  Can_GetControllerErrorState/Mode              │
    │  └── 封装: NXP CAN PAL (can_pal.h)                      │
    └──────────────────────┬──────────────────────────────────┘
                           │
                    ┌──────▼──────┐
                    │ NXP CAN PAL │
                    │ (can_pal.h) │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  FlexCAN 0  │
                    │  (硬件 IP)  │
                    └──────┬──────┘
                           │
                     CAN_H │ CAN_L
                    ───────┴───────
                       CAN 总线
```

---

## 参考文件

| 文件 | 内容 |
|------|------|
| `mcu/include/ComStack_Types.h` | **全栈共享** `PduInfoType` / `PduIdType` / `PduLengthType` |
| `mcu/RTE/Rte.h` | Runtime Environment：SWC↔BSW 信号映射 |
| `mcu/MCAL/Can/include/Can.h` | MCAL Can 驱动 + `Can_IdType`/`Can_HwHandleType`/`Can_ErrorStateType` |
| `mcu/MCAL/Can/src/Can.c` | MCAL Can 驱动实现（AUTOSAR 标准 `Can_Write(Hth, Pdu)` 签名） |
| `mcu/EcuAbstraction/CanIf/include/CanIf.h` | CanIf 头文件（使用 `PduInfoType*`，AUTOSAR 标准） |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | CanIf 实现：N-PDU → L-PDU 格式转换 + DET |
| `mcu/Services/CanTp/include/CanTp.h` | CanTp API + PCI 编解码 |
| `mcu/Services/CanTp/src/CanTp.c` | N-PDU 处理核心：SF/FF/CF/FC 状态机 |
| `mcu/Services/PduR/include/PduR.h` | PduR I-PDU 路由（使用 `PduInfoType*`） |
| `mcu/Services/Com/include/Com.h` | Com API 声明 + 类型定义 |
| `mcu/Services/Com/src/Com.c` | Com 完整实现（信号编解码 + Update Bit + Timeout + DET） |
| `mcu/Services/Com/config/Com_Cfg.h` | Com 配置表（信号位布局 + I-PDU 周期 + 超时） |
| `mcu/Services/Com/config/Com_Cfg.c` | Com 配置实例 |
| `mcu/Services/Det/include/Det.h` | DET 错误追踪头文件 |
| `mcu/Services/Det/src/Det.c` | DET 实现（错误→UART 日志） |
| `mcu/App/Swc_SignalGateway/src/main.c` | 应用层 CAN 测试程序（走完整 AUTOSAR 链路） |
