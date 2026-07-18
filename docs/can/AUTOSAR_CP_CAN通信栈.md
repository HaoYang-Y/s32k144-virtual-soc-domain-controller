# AUTOSAR CP CAN 通信栈

> 以本项目的 MCU 端代码为例，逐层讲解 AUTOSAR Classic Platform 的 CAN 通信栈
> 架构、各层职责、API 调用链、数据收发路径，以及当前实现状态。

---

## 1. 整体架构：五层模型

AUTOSAR CP 将 CAN 通信拆分为五个层次,从应用信号到物理总线逐层向下：

```
 ┌──────────────────────────────────────────────────────────────────────┐
 │                      SWC (应用软件组件)                               │
 │                   mcu/App/Swc_SignalGateway/src/main.c               │
 ├──────────────────────────────────────────────────────────────────────┤
 │                                                                      │
 │  ┌─────────────────────────────────────────────────────────────┐    │
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
 │                         │ PduR → CanTp (可选) → CanIf                │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    CanTp (传输层) [可选]                      │    │
 │  │          mcu/Services/CanTp/include/CanTp.h                   │    │
 │  │  · ISO 15765-2 多帧分包/重组                                 │    │
 │  │  · CanTp_Transmit() → SF / FF+CF+FC                          │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ CanTp → CanIf                              │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    CanIf (CAN 接口)                           │    │
 │  │          mcu/EcuAbstraction/CanIf/include/CanIf.h             │    │
 │  │  · 硬件无关的 CAN 收发抽象                                    │    │
 │  │  · CanIf_Transmit() / CanIf_RxIndication()                   │    │
 │  └──────────────────────┬──────────────────────────────────────┘    │
 │                         │ CanIf → Can (MCAL)                         │
 │  ┌──────────────────────▼──────────────────────────────────────┐    │
 │  │                    Can (MCAL 驱动)                            │    │
 │  │              mcu/MCAL/Can/include/Can.h                       │    │
 │  │  · 直接硬件访问 (CAN PAL → FlexCAN 寄存器)                    │    │
 │  │  · Can_Write() / Can_Read() → 物理 CAN 帧                    │    │
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

### 2.1 Can — MCAL 驱动层（已实现）

**职责**：直接访问 CAN 硬件（FlexCAN 控制器），发送/接收物理 CAN 帧。

**文件位置**：
- `mcu/MCAL/Can/include/Can.h` — 类型定义和 API 声明
- `mcu/MCAL/Can/src/Can.c` — 驱动实现（封装 NXP CAN PAL）

**核心类型**：

```c
// Can.h 第 47~54 行 — CAN PDU 数据单元
typedef struct {
    uint32_t id;           // CAN ID (11-bit 或 29-bit)
    uint8_t  length;       // 数据长度 (0~8)
    bool     is_extended;  // 是否扩展帧
    bool     is_remote;    // 是否远程帧
    uint8_t  data[8];      // 数据载荷
} Can_PduType;

// Can.h 第 60~82 行 — 驱动配置
typedef struct {
    uint8_t       controller;        // 控制器编号
    uint8_t       max_num_mb;        // 最大 Mailbox 数
    bool          is_rx_fifo_needed; // 是否使用 RX FIFO
    Can_ModeType  flexcan_mode;      // NORMAL / LOOPBACK
    uint8_t       prop_seg;          // 位时序: PROP_SEG
    uint8_t       phase_seg1;        // 位时序: PHASE_SEG1
    uint8_t       phase_seg2;        // 位时序: PHASE_SEG2
    uint8_t       pre_divider;       // 波特率预分频
    uint8_t       r_jumpwidth;       // SJW
    uint8_t       num_tx_mailboxes;  // TX Mailbox 数量
    uint8_t       num_rx_mailboxes;  // RX Mailbox 数量
    const Can_HardwareObject *tx_mailboxes;  // TX MB 列表
    const Can_HardwareObject *rx_mailboxes;  // RX MB 列表
} Can_ConfigType;
```

**核心 API**：

```c
// Can.h 第 88~95 行
status_t  Can_Init(const Can_ConfigType *ConfigPtr);
void      Can_DeInit(void);
void      Can_SetControllerMode(Can_ControllerType Controller,
                                Can_ControllerStateType Transition);
status_t  Can_Write(uint8_t Controller, uint8_t Hth, const Can_PduType *PduInfo);
status_t  Can_Read(uint8_t Controller, uint8_t Hrh, Can_PduType *PduInfo);
```

**实现要点**（`Can.c`）：

1. **MCAL 配置 → PAL 配置转换**（`Can_BuildPalConfig()`, 第 41~61 行）：
   - 位时序字段直接映射到 `can_user_config_t.nominalBitrate`
   - `enableFD = false`, `payloadSize = CAN_PAYLOAD_SIZE_8`（经典 CAN）
   - `peClkSrc = CAN_CLK_SOURCE_OSC`

2. **初始化流程**（`Can_Init()`, 第 67~111 行）：
   ```
   Can_Init()
     ├── 保存 MCAL 配置副本
     ├── Can_BuildPalConfig() → can_user_config_t
     ├── CAN_Init(&Can_Instance, &Can_PalConfig)     // PAL 初始化（内部操作 FlexCAN 寄存器）
     ├── for (TX MBs): CAN_ConfigTxBuff()              // 配置发送 Mailbox
     └── for (RX MBs): CAN_ConfigRxBuff()              // 配置接收 Mailbox（设置过滤 ID）
   ```

3. **发送流程**（`Can_Write()`, 第 147~171 行）：
   ```
   Can_Write()
     ├── 参数校验（初始化/控制器/状态/Hth/长度）
     ├── CAN_ConfigTxBuff()  ← 关键：每次发送前必须重配 TX MB
     │                         原因：上次发送后 MB 状态不为空闲，
     │                         PAL 内部 FLEXCAN_DRV_Send 的状态检查
     │                         会因 MB 非空闲而返回 BUSY
     ├── 构造 can_message_t (id, length, data[0..7])
     └── CAN_Send(&Can_Instance, Hth, &tx_msg)
   ```

4. **接收流程**（`Can_Read()`, 第 177~201 行）：
   ```
   Can_Read()
     ├── 参数校验
     ├── CAN_Receive(&Can_Instance, Hrh, &rx_msg)  // MB 索引 = TX 数量 + RX 偏移
     └── 填充 Can_PduType (id, length, is_extended, data[0..7])
   ```

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

### 2.2 CanIf — CAN Interface 层（骨架）

**职责**：在 MCAL Can 驱动之上提供**硬件无关**的 CAN 通信接口。
对上（PduR/CanTp）隐藏具体硬件细节，对下（Can）统一调用 MCAL 接口。

**文件位置**：
- `mcu/EcuAbstraction/CanIf/include/CanIf.h` — 类型定义和 API 声明
- `mcu/EcuAbstraction/CanIf/src/CanIf.c` — 当前为骨架实现

**核心 API**：

```c
// CanIf.h 第 41~56 行
void    CanIf_RxIndication(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr);
void    CanIf_TxConfirmation(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr);
uint8_t CanIf_Transmit(CanIf_ControllerType Controller, CanIf_PduType *PduPtr);
void    CanIf_Init(void);
```

**当前状态**：骨架（SKELETON）。

`CanIf.c` 已实现：
- 模块初始化（`CanIf_Init`, 第 39~43 行）
- AUTOSAR 风格错误检测框架（DET, CanIf.c 第 18~32 行）
  - `CANIF_MODULE_ID = 0x32`、开发错误检测、参数空指针检查、未初始化检查

`CanIf.c` 待实现（标注 TODO）：
- `CanIf_Transmit` → 调用 `Can_Write`（CanIf.c 第 106 行）
- `CanIf_RxIndication` → 转发给 `PduR_CanIfRxIndication`（CanIf.c 第 63 行）
- `CanIf_TxConfirmation` → 通知 PduR 发送完成（CanIf.c 第 84 行）

---

### 2.3 CanTp — CAN Transport Layer（骨架）

**职责**：实现 ISO 15765-2 多帧传输协议。当 PDU 超过 8 字节时进行分包/重组。

**文件位置**：`mcu/Services/CanTp/include/CanTp.h`

**核心概念**：

```c
// CanTp.h 第 22~27 行 — 四种帧类型
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

**当前状态**：骨架（SKELETON），仅有类型定义和函数声明。

---

### 2.4 PduR — PDU Router（骨架）

**职责**：在通信协议栈中**路由 I-PDU**。典型的 AUTOSAR 系统中，PduR 负责在
Com、CanTp、SpiIf（CAN 之外的通信总线接口）等模块之间转发 PDU。

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
// PduR.h 第 22~25 行
void    PduR_Init(void);
uint8_t PduR_ComTransmit(PduR_PduIdType PduId, const PduR_InfoType *PduInfo);
void    PduR_CanIfRxIndication(PduR_PduIdType RxPduId, const PduR_InfoType *PduInfo);
void    PduR_CanIfTxConfirmation(PduR_PduIdType TxPduId);
```

**当前状态**：骨架（SKELETON），仅有类型定义和函数声明。

---

### 2.5 Com — Communication Service（骨架）

**职责**：AUTOSAR 通信栈的最顶层。负责信号的打包/解包、发送模式控制
（DIRECT/NONE/PERIODIC/MIXED）、信号门控等。

**文件位置**：`mcu/Services/Com/include/Com.h`

**核心概念**：

```
    SWC (main.c)
    Com_SendSignal(SignalId, &value)
           │
           ▼
    Com 模块内部
    ├── 将 Signal 值按位布局写入 I-PDU 缓冲区 (packing)
    └── 根据传输模式触发 I-PDU 发送
           │
           ▼
    PduR_ComTransmit(I-PduId, &PduInfo)
```

**核心 API**：

```c
// Com.h 第 34~37 行 (实际为 31~37 行)
void Com_Init(void);
void Com_MainFunction(void);
void Com_SendSignal(Com_SignalIdType SignalId, const void *SignalData);
void Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalData);
```

**当前状态**：骨架（SKELETON），仅有类型定义和函数声明。

---

## 3. 数据收发完整路径

### 3.1 发送路径（TX，自上而下）

```
SWC/App
  │
  │ Com_SendSignal(SignalId, &value)
  ▼
Com: 信号 → PDU 打包
  │ 将信号值按位布局写入 I-PDU 缓冲区
  │ 根据传输模式决定是否触发发送
  │
  │ PduR_ComTransmit(PduId, &PduInfo)
  ▼
PduR: 查找路由表
  │ 根据 PduId 决定目标: CanTp (CAN 路径) 或 SpiIf (SPI 路径)
  │
  │ CanTp_Transmit(PduId, &PduInfo)
  ▼
CanTp: 多帧分包 (ISO 15765-2)
  │ 若 data_len ≤ 7: 直接发 Single Frame
  │ 若 data_len > 7: 分为 FF + 多个 CF, 等待 FC 流控
  │
  │ CanIf_Transmit(Controller, &Pdu)
  ▼
CanIf: PDU → CAN 帧映射
  │ 根据 PDU ID 查找 CAN ID 和 Hth (Hardware Transmit Handle)
  │
  │ Can_Write(Controller, Hth, &CanPdu)
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
  │ 填充 Can_PduType (id, length, data)
  │
  │ CanIf_RxIndication(Controller, &Pdu)
  ▼
CanIf: CAN 帧 → PDU 映射
  │ 根据 CAN ID 查找 PDU ID
  │
  │ PduR_CanIfRxIndication(RxPduId, &PduInfo)
  ▼
PduR: 查找路由表
  │ 根据 RxPduId 决定目标: Com (数据) 或 CanTp (需要重组的多帧)
  │
  │ ┌─ 单帧路径 ─→ Com_RxIndication(PduId, &PduInfo)
  │ └─ 多帧路径 ─→ CanTp_RxIndication(PduId, &Data)
  ▼
CanTp (多帧): 重组 CF 帧 → 完整 PDU → PduR → Com

Com: PDU → 信号解包
  │ 从 I-PDU 缓冲区中按位布局提取各 Signal 值
  │
  │ Com_ReceiveSignal(SignalId, &value) (SWC 主动读取)
  ▼
SWC/App
```

---

## 4. 当前项目实现状态

### 4.1 状态总览

```
    Com:    [SKELETON] 仅有类型和函数声明，无实现
    PduR:   [SKELETON] 仅有类型和函数声明，无实现
    CanTp:  [SKELETON] 仅有类型和函数声明，无实现
    CanIf:  [SKELETON] 头文件完整，源文件有 DET 框架但收发逻辑未实现
    Can:    [已实现]    基于 NXP CAN PAL 的完整 MCAL 驱动
```

### 4.2 当前实际使用的数据路径

由于 CanIf/PduR/Com 尚未实现，当前 `main.c` **直接调用 MCAL Can 接口**：

```c
// mcu/App/Swc_SignalGateway/src/main.c (第 41~59 行)
// 当前跳过了 Com/PduR/CanTp/CanIf 四层，直接操作 MCAL

for (;;) {
    // TX: 直接构造 Can_PduType → Can_Write
    Can_PduType tx = {.id = 0x123UL, .length = 8U};
    tx.data[0] = cnt & 0xFF;
    // ...
    status_t s = Can_Write(0, TX_MB, &tx);

    // RX: 直接 Can_Read
    Can_PduType rx;
    if (Can_Read(0, RX_MB, &rx) == STATUS_SUCCESS) {
        // 处理接收数据
    }
}
```

### 4.3 向完整 AUTOSAR 栈迁移的路线图

当需要完整 AUTOSAR Com 栈时（如需要信号级通信、多帧 OTA、UDS 诊断等），
按以下顺序实现：

1. **CanIf → Can 桥接**：实现 `CanIf_Transmit()` 调用 `Can_Write()`，
   实现 `CanIf_RxIndication()` 的接收回调机制

2. **PduR 路由表**：实现 `PduR_ComTransmit()` 路由到 CanIf，
   实现 `PduR_CanIfRxIndication()` 路由到 Com

3. **Com 信号打包**：实现信号到 PDU 的位布局，实现 `Com_SendSignal()` 和
   `Com_ReceiveSignal()`

4. **CanTp 多帧**（可选）：实现 ISO 15765-2 的 SF/FF/CF/FC 处理

---

## 5. 关键设计要点

### 5.1 模块 ID 与错误检测

项目中遵循 AUTOSAR DET（Development Error Tracer）规范，每层分配唯一的模块 ID：

| 模块 | Module ID (AUTOSAR 规范) | 文件 |
|------|-------------------------|------|
| CanIf | 0x32 (50) | `CanIf.c` 第 18 行 |

各模块的 API ID 用于精确定位错误来源（`CanIf.c` 第 22~26 行）：

```c
#define CANIF_INIT_ID          0x00U
#define CANIF_TRANSMIT_ID      0x01U
#define CANIF_RX_INDICATION_ID 0x02U
#define CANIF_TX_CONFIRM_ID    0x03U
```

### 5.2 每次 Can_Write 前必须重配 TX MB

这是该项目 Can.c 最重要的一个工程技巧（`Can.c` 第 142~145 行注释 + 第 160 行代码）：

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
│  [当前] 直接调用 Can_Write() / Can_Read()                     │
│  [未来] 通过 Com_SendSignal() / Com_ReceiveSignal()           │
└──────────────────────────┬────────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │                Services Layer                           │
    │                                                         │
    │  Com (mcu/Services/Com)                                 │
    │  ├── 类型: Com_SignalIdType, Com_SignalType             │
    │  └── API:  Com_SendSignal(), Com_ReceiveSignal()  [骨架] │
    │                                                         │
    │  PduR (mcu/Services/PduR)                               │
    │  ├── 类型: PduR_PduIdType, PduR_InfoType                │
    │  └── API:  PduR_ComTransmit(), PduR_CanIfRxIndication() │
    │            [骨架]                                        │
    │                                                         │
    │  CanTp (mcu/Services/CanTp)                             │
    │  ├── 类型: CanTp_FrameType (SF/FF/CF/FC)                │
    │  └── API:  CanTp_Transmit(), CanTp_RxIndication() [骨架] │
    └──────────────────────┬──────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │            ECU Abstraction Layer                        │
    │                                                         │
    │  CanIf (mcu/EcuAbstraction/CanIf)                       │
    │  ├── 类型: CanIf_ControllerType, CanIf_PduType          │
    │  ├── API:  CanIf_Transmit(), CanIf_RxIndication()       │
    │  └── DET:  ModuleId=0x32, ApiId, ErrorId         [骨架] │
    └──────────────────────┬──────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────────────────┐
    │                MCAL Layer                               │
    │                                                         │
    │  Can (mcu/MCAL/Can)                                       │
    │  ├── 类型: Can_ConfigType, Can_PduType                  │
    │  ├── API:  Can_Init(), Can_Write(), Can_Read()   [已实现] │
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

| 文件 | 行数（关键） | 内容 |
|------|-------------|------|
| `mcu/MCAL/Can/include/Can.h` | 41~54 (类型), 60~82 (配置), 88~95 (API) | MCAL Can 驱动头文件 |
| `mcu/MCAL/Can/src/Can.c` | 41~61 (配置转换), 67~111 (初始化), 147~171 (发送), 177~201 (接收) | MCAL Can 驱动实现 |
| `mcu/EcuAbstraction/CanIf/include/CanIf.h` | 20~30 (类型), 41~56 (API) | CanIf 头文件 |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | 18~32 (DET), 39~43 (初始化), 87~108 (发送) | CanIf 骨架实现 |
| `mcu/Services/CanTp/include/CanTp.h` | 15~33 | CanTp 类型定义 |
| `mcu/Services/PduR/include/PduR.h` | 14~25 | PduR 类型定义 |
| `mcu/Services/Com/include/Com.h` | 18~42 | Com 类型定义 |
| `mcu/App/Swc_SignalGateway/src/main.c` | 17~63 | 应用层 CAN 测试程序 |
