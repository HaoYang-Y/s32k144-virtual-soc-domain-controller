# MCAL Can 驱动详解

> 从零开始，逐函数讲解本项目的 MCAL Can 驱动——如何用 AUTOSAR 标准接口封装 NXP S32K144 FlexCAN 硬件。

---

## 阅读前你需要知道

本文假设你：

- 知道 CAN 总线是什么（物理层：两根差分线 CAN_H/CAN_L；帧结构：ID + DLC + 最多 8 字节数据）
- 知道 AUTOSAR 五层模型的大概样子

如果还不清楚，先看这几篇：

| 前置知识 | 文档 |
|----------|------|
| CAN 协议基础 | [../CAN原理/1_标准CAN协议入门.md](../CAN原理/1_标准CAN协议入门.md) |
| CAN 帧如何被硬件收发（Mailbox） | [../CAN原理/5_CAN邮箱详解.md](../CAN原理/5_CAN邮箱详解.md) |
| AUTOSAR 五层全景 | [./1_AUTOSAR_CP_CAN通信栈.md](./1_AUTOSAR_CP_CAN通信栈.md) |

---

## 1. 什么是 MCAL？为什么需要它？

MCAL（Microcontroller Abstraction Layer，微控制器抽象层）是 AUTOSAR 五层模型的最底层。它的职责只有一句话：

> **把 MCU 厂商的硬件操作，包装成 AUTOSAR 标准的函数名和参数格式。**

举个具体例子。NXP 的 SDK 里，发送一帧 CAN 数据要这样写：

```c
// NXP SDK 原生写法（上层代码直接依赖 NXP 头文件）
can_message_t msg = {.id = 0x123, .length = 8, .data = {...}};
CAN_Send(&canInstance, mailboxIndex, &msg);
```

如果哪天换成了 ST 的 MCU，这段代码就全废了——ST 的函数名、参数、类型都不一样。

MCAL 的做法是：定义一个 AUTOSAR 标准接口，内部调 NXP SDK，但对外**不暴露任何 NXP 头文件**：

```c
// AUTOSAR MCAL 写法（上层代码不依赖任何厂商 SDK）
Can_PduType pdu = {.id = 0x123, .length = 8, .data = {...}};
Can_Write(Controller, Hth, &pdu);
```

换 MCU 时只改 `Can.c` 内部实现，上层代码（CanIf → PduR → Com → RTE → SWC）一行不用动。

```
 SWC / RTE / Com / PduR / CanIf        ← 这些层完全不知道用的是 NXP 还是 ST
══════════════════════════════════════
 Can.h (AUTOSAR 标准接口)              ← 本文详解
 Can.c (内部调 NXP CAN PAL)            ← 本文详解
══════════════════════════════════════
 NXP CAN PAL (can_pal.h)               ← NXP 专有
 FlexCAN 硬件寄存器                     ← NXP 专有
```

---

## 2. 硬件基础：FlexCAN Mailbox

在深入代码之前，必须理解一个硬件概念——**Mailbox（MB，消息邮箱）**。FlexCAN 是 NXP S32K144 片内的 CAN 控制器 IP。

每个 MB 是一个独立的硬件缓冲单元，由两个关键编号定位：

- **HTH**（Hardware Transmit Handle）— 发送用的 MB 索引，`Can_Write(Controller, Hth, &pdu)` 的第二个参数
- **HRH**（Hardware Receive Handle）— 接收用的 MB 索引，`Can_Read(Controller, Hrh, &pdu)` 的第二个参数

S32K144 的 FlexCAN 控制器有 **16 个 Mailbox**，每个 MB 是一个独立的硬件缓冲单元。你可以把 MB 想象成停车位：

```
MB 索引:  0         1         2..15
         ┌────┐   ┌────┐   ┌──────────┐
         │ TX │   │ RX │   │  未使用    │
         │车位 │   │车位 │   │          │
         └────┘   └────┘   └──────────┘
         发 0x123  收 0x100
```

关键规则：

| 规则 | 说明 |
|------|------|
| **TX MB 从索引 0 开始** | 发送邮箱占据低编号 |
| **RX MB 紧接 TX 之后** | 索引 = `num_tx` 起，每个 RX MB 绑定一个 CAN ID 过滤器 |
| **每个 MB 同一时刻只能一个方向** | 要么 TX 要么 RX，不能同时 |
| **RX MB 按 ID 过滤** | 只有匹配的 CAN ID 的帧才会被存入该 MB |

本项目的配置：

```c
static const Can_HardwareObject tx_mb[] = {{.id = 0x123UL}};   // TX MB 0: 发 CAN ID 0x123
static const Can_HardwareObject rx_mb[] = {{.id = 0x100UL}};   // RX MB 1: 收 CAN ID 0x100
//                                      num_tx=1  num_rx=1
```

---

## 3. 核心类型

### 3.1 Can_PduType — 一帧 CAN 数据

```c
// Can.h
typedef struct {
    Can_IdType   id;          // CAN ID — AUTOSAR 标准类型 Can_IdType (uint32_t)
    uint8_t      length;      // 数据长度 (0~8 字节)
    bool         is_extended; // true = 扩展帧 (29-bit ID), false = 标准帧 (11-bit)
    bool         is_remote;   // true = 远程帧 (请求数据，极少用)
    uint8_t      data[8];     // 8 字节数据载荷 — L-PDU 定长数组
} Can_PduType;
```

> ⚠️ **踩坑记录**：`is_extended` 和 `is_remote` 必须显式初始化为 `false`。栈上不初始化这两个 `bool` 字段，垃圾值可能导致标准帧被当成扩展帧发出。构造 `Can_PduType` 时务必使用 `= {0}` 全量零初始化。

### 3.2 Can_HardwareObject — Mailbox 定义

```c
// Can.h
typedef struct {
    uint32_t id;           // 该 MB 绑定的 CAN ID
    bool     is_extended;  // 是否匹配扩展帧
    bool     is_remote;    // 是否匹配远程帧
} Can_HardwareObject;
```

TX MB 的 `id` 仅作标识；RX MB 的 `id` 是**硬件过滤器**——只有此 ID 的帧才会被存入。

### 3.3 Can_ConfigType — 初始化配置

```c
// Can.h
typedef struct {
    uint8_t       controller;         // 控制器编号 (0 = FlexCAN0)
    uint8_t       max_num_mb;         // 最大 Mailbox 数 (S32K144 = 16)
    Can_ModeType  flexcan_mode;       // NORMAL / LOOPBACK / FREEZE

    // ── 位时序（决定波特率）──
    uint8_t       prop_seg;           // 传播段
    uint8_t       phase_seg1;         // 相位段 1
    uint8_t       phase_seg2;         // 相位段 2
    uint8_t       pre_divider;        // 时钟预分频
    uint8_t       r_jumpwidth;        // 同步跳转宽度 (SJW)

    // ── Mailbox 分配 ──
    uint8_t       num_tx_mailboxes;         // TX MB 个数
    uint8_t       num_rx_mailboxes;         // RX MB 个数
    const Can_HardwareObject *tx_mailboxes; // TX MB 列表
    const Can_HardwareObject *rx_mailboxes; // RX MB 列表（含过滤 ID）
} Can_ConfigType;
```

### 3.4 Can_ControllerStateType — 控制器状态机

这是 AUTOSAR SWS_Can 定义的标准状态机：

```
                    Can_Init()
  CAN_CS_UNINIT ────────────────→ CAN_CS_STOPPED
       ▲                                │
       │  Can_DeInit()         Can_SetControllerMode(STARTED)
       │                                │
       └────────────────────────────────↓
                                    CAN_CS_STARTED
                                    (允许 Write/Read)
```

```c
typedef enum {
    CAN_CS_UNINIT  = 0,   // 未初始化 — 上电默认
    CAN_CS_STARTED = 1,   // 运行中 — Can_Write/Can_Read 可用
    CAN_CS_STOPPED = 2,   // 已初始化但暂停 — 只有 Init 完成，未 Start
    CAN_CS_SLEEP   = 3,   // 休眠（本项目未使用）
} Can_ControllerStateType;
```

**Can_Write 和 Can_Read 内部第一件事就是检查 `Can_State == CAN_CS_STARTED`**，不是 STARTED 直接返回 ERROR。

### 3.6 AUTOSAR 标准类型

对标 AUTOSAR SWS_Can 规范，本模块新增了三个标准类型：

```c
// Can.h

/** CAN 消息 ID 类型 (SWS_Can_00008) */
typedef uint32_t Can_IdType;

/** Hardware Object Handle (SWS_Can_00009)
 *  用于 Can_Write 的 HTH 和 Can_Read 的 HRH，替代裸 uint8_t */
typedef uint16_t Can_HwHandleType;

/** CAN 控制器错误状态 (SWS_Can_00016) */
typedef enum {
    CAN_ERRORSTATE_ACTIVE  = 0,   // Error Active  — 正常通信
    CAN_ERRORSTATE_PASSIVE = 1,   // Error Passive — 可通信但受限
    CAN_ERRORSTATE_BUSOFF  = 2,   // Bus Off       — 脱离总线
} Can_ErrorStateType;
```

**为什么需要这些类型？**
- `Can_IdType`：替代裸 `uint32_t`，语义更清晰——"这是一个 CAN ID"
- `Can_HwHandleType`：替代裸 `uint8_t`，且扩展为 `uint16_t`——当有多控制器时，高字节可编码控制器索引
- `Can_ErrorStateType`：支持 `Can_GetControllerErrorState()` 返回值语义化

### 3.7 AUTOSAR 标准 API

```c
/** 查询控制器错误状态 (SWS_Can_00167) */
Std_ReturnType Can_GetControllerErrorState(Can_ControllerType  Controller,
                                           Can_ErrorStateType *ErrorStatePtr);

/** 查询控制器当前模式 (SWS_Can_00130) */
Std_ReturnType Can_GetControllerMode(Can_ControllerType      Controller,
                                     Can_ControllerStateType *ModePtr);
```

当前实现返回内部维护的静态状态值，后续可扩展为读取 FlexCAN 硬件寄存器（ESR1）。

---

## 4. AUTOSAR 定义的两种收发方式

AUTOSAR SWS_Can 规范定义了 CAN 驱动的两种运行模式，区别在于**谁来触发收发动作**：

### 4.1 轮询模式（Polling）— 当前使用

```
main() 循环:
  │
  ├── Can_Write(MB0, &tx)        ← 主动调用，立即提交到硬件
  │
  ├── Can_Read(0, MB1, &rx)      ← 主动调用，当场检查"有没有新帧？"
  │     有 → 返回数据
  │     无 → 返回 STATUS_ERROR
  │
  └── delay_ms(500)
```

**特点**：CPU 自己周期性地去"敲门问有没有新数据"。简单直接，但有两个问题：

1. **CPU 空转**：两帧之间 CPU 在 delay 中什么也不干
2. **丢帧风险**：如果一帧到达后、下次 `Can_Read` 前又有新帧覆盖了 MB，第一帧就丢了

### 4.2 中断模式（Interrupt）— AUTOSAR 推荐

```
硬件中断触发:
  CAN 帧到达 MB → 硬件触发 CAN0_ORed_0_15_MB_IRQHandler
    │
    ├── FLEXCAN_IRQHandler(0)     ← NXP SDK ISR，扫描所有 MB
    │      │
    │      └── 回调: Can_IrqCallback(RX_COMPLETE, mb_idx)
    │             │
    │             ├── Can_Read(0, mb_idx, &rx)   ← 在 ISR 中读数据
    │             └── CanIf_RxIndication(&rx)     ← 通知上层
    │
    └── 重新武装 RX MB（准备收下一帧）

main() 循环:
  │
  ├── Can_Write(MB0, &tx)        ← TX 不变（本来异步）
  │
  └── Can_MainFunctionRead()     ← AUTOSAR 要求周期性调，处理 ISR 缓冲的数据
```

**特点**：

1. **零延迟响应**：帧到达后硬件立即触发 ISR，不依赖 CPU 轮询间隔
2. **不丢帧**：ISR 第一时间读出数据，MB 立即重新武装
3. **AUTOSAR 标准模式**：规范要求 `Can_MainFunctionRead/Write` 作为周期任务运行

### 4.3 两种模式对比

| | 轮询 | 中断 |
|--|------|------|
| 触发方式 | CPU 周期性检查 | 硬件自动触发 ISR |
| CPU 效率 | 低（空转等待） | 高（ISR 只在有数据时运行） |
| 实时性 | 取决于轮询间隔 | 微秒级响应 |
| 丢帧风险 | 有（两帧间隔 < 轮询间隔） | 极低（ISR 及时读出） |
| AUTOSAR 规范 | 允许，但不推荐 | 推荐（SWS_Can_00047/00048） |
| 实现复杂度 | 简单 | 需要安装回调 + 武装 MB + ISR 安全 |
| 本项目状态 | ✅ 已实现（main.c while 循环） | ⬜ 方案已定（见 [中断模式迁移方案](./中断模式迁移方案.md)） |

### 4.4 AUTOSAR 的中断模式 API

规范定义了三个用于中断模式的函数（本项目待实现）：

```c
// SWS_Can_00047: 周期调用，处理 TX 完成确认
void Can_MainFunctionWrite(void);

// SWS_Can_00048: 周期调用，处理 ISR 缓冲的 RX 数据
void Can_MainFunctionRead(void);

// SWS_Can_00046: 启用/禁用特定 MB 的中断（AUTOSAR 标准 API）
void Can_EnableCanInterrupts(uint8_t Controller, uint8_t Hoh);

// 注意：本项目计划用 Can_EnableInterrupts() 封装上述标准 API，
// 内部调用 CAN_InstallEventCallback + 武装 RX MB。详见中断迁移方案。
```
```

**AUTOSAR 典型中断收发流程**：

```
初始化:
  Can_Init() → Can_SetControllerMode(STARTED) → Can_EnableInterrupts()
  → 安装回调 → 武装 RX MB → 使能 NVIC

运行时:
  ┌─ ISR 上下文 ─────────────────────────────┐
  │ 帧到达 → FLEXCAN_IRQHandler → 回调        │
  │   → 读 MB 数据 → 存到软件 FIFO            │
  │   → CanIf_RxIndication(PDU_ID, &data)     │
  │   → 重新武装 MB                            │
  └──────────────────────────────────────────┘

  ┌─ main() 循环 ────────────────────────────┐
  │ Can_MainFunctionRead()  ← 处理软件 FIFO   │
  │ Can_MainFunctionWrite() ← 处理 TX 确认    │
  │ ... 业务逻辑 ...                           │
  └──────────────────────────────────────────┘
```

> **当前状态**：本项目第 4.2/4.3 节的 `Can_Write`/`Can_Read` 是**轮询模式**的实现。中断模式的详细迁移方案见 [中断模式迁移方案](./中断模式迁移方案.md)。

---

## 5. 核心 API

### 5.1 Can_Init — 对标 AUTOSAR SWS_Can_00013

```c
Std_ReturnType Can_Init(const Can_ConfigType *ConfigPtr);
```

> **AUTOSAR 规范定义**：初始化 CAN 控制器硬件，配置位时序和 Mailbox。成功后控制器进入 CAN_CS_STOPPED 状态。每个控制器只能 Init 一次，重复 Init 必须先 DeInit。

参数 `ConfigPtr` 就是上一节说的 `Can_ConfigType`，包含所有硬件参数。

**内部流程**：

```
Can_Init(ConfigPtr)
  │
  ├── 1. 保存配置副本到模块静态变量
  │      Can_Config = *ConfigPtr  (值拷贝，不持有指针)
  │      Can_TxCount = num_tx_mailboxes
  │      Can_RxCount = num_rx_mailboxes
  │
  ├── 2. Can_BuildPalConfig()   ← MCAL 配置 → PAL 配置 转换（内部函数）
  │      · 位时序: prop_seg / phase_seg1 / phase_seg2 / pre_divider → nominalBitrate
  │      · 模式:   NORMAL / LOOPBACK
  │      · 经典CAN: enableFD = false, payloadSize = CAN_PAYLOAD_SIZE_8
  │      · 时钟源:  CAN_CLK_SOURCE_OSC (PE 直连外部 8MHz 晶振)
  │
  ├── 3. CAN_Init(&Can_Instance, &Can_PalConfig)    // 调 NXP PAL 初始化硬件
  │      内部过程: 使能 FlexCAN 时钟 → 进入 Freeze 模式 → 写位时序寄存器
  │      → 设置 MB 数量 → 退出 Freeze 模式
  │
  ├── 4. 配置 TX Mailbox (索引 0 .. num_tx-1)
  │      for (i = 0; i < num_tx; i++)
  │          CAN_ConfigTxBuff(&Can_Instance, i, &Can_TxBuffCfg)
  │
  ├── 5. 配置 RX Mailbox (索引 num_tx .. num_tx+num_rx-1)
  │      for (i = 0; i < num_rx; i++)
  │          CAN_ConfigRxBuff(&Can_Instance, num_tx + i, &Can_RxBuffCfg, rx_mb[i].id)
  │                                          ↑ 绝对索引               ↑ 过滤 ID
  │
  └── 6. Can_Initialized = true
         Can_State = CAN_CS_STOPPED  (需再调 SetControllerMode(STARTED) 才能收发)
```

**配置定义**（不在 main.c 中，而在专用配置文件内）：

```c
// Can_Cfg.c — CAN 驱动配置实例（供 EcuM 引用）
#include "Can_Cfg.h"

static const Can_HardwareObject tx_mb[] = {{.id = 0x123UL}};
static const Can_HardwareObject rx_mb[] = {{.id = 0x100UL}};

const Can_ConfigType Can_Config = {
    .controller       = 0,
    .max_num_mb       = 16,
    .flexcan_mode     = CAN_MODE_NORMAL,
    .prop_seg         = 7,
    .phase_seg1       = 4,
    .phase_seg2       = 1,
    .pre_divider      = 0,
    .r_jumpwidth      = 1,
    .num_tx_mailboxes = 1,
    .num_rx_mailboxes = 1,
    .tx_mailboxes     = tx_mb,
    .rx_mailboxes     = rx_mb,
};
```

**初始化**（由 EcuM 统一调度，不在 main.c 中直接调用）：

```c
// EcuM.c — EcuM_Init() 内部按 AUTOSAR 顺序调用
if (Can_Init(&Can_Config) != E_OK) {
    return;                                  // 初始化失败，进入安全状态
}
(void)Can_SetControllerMode(CAN_CONTROLLER_0, CAN_CS_STARTED);
```

```c
// main.c — 应用层只需调用 EcuM_Init()
EcuM_Init();  // 内部依次初始化 MCAL → ECU Abstraction → Services → RTE
```

---

### 5.2 Can_Write — 对标 AUTOSAR SWS_Can_00106

```c
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
```

> **AUTOSAR 规范定义**：将一个 CAN L-PDU 写入由 Hth（Hardware Transmit Handle）标识的 TX 硬件对象。Controller 由驱动内部根据配置维护，不暴露给调用方。函数立即返回，不等待发送完成。

| 参数 | 含义 | 本项目实际值 |
|------|------|-------------|
| `Hth` | TX Hardware Transmit Handle（Mailbox 索引） | 0 = TX_MB（唯一 TX MB） |
| `PduInfo` | 指向待发送 L-PDU | `{id=0x123, length=8, data[8]=...}` |

返回值：`E_OK`（发送请求已提交）或 `E_NOT_OK`（参数错误 / 控制器未启动）。

**内部流程**：

```
Can_Write(Hth=0, PduInfo)
  │
  ├── 1. 参数校验（5 项全通过才继续，Controller 由内部获取）
  │      · Can_Initialized == true ?
  │      · Can_State[Can_Config.controller] == CAN_CS_STARTED ?
  │      · Hth < Can_TxCount ?
  │      · PduInfo != NULL ?
  │      · PduInfo->length ≤ 8 ?
  │
  ├── 2. CAN_ConfigTxBuff(&Can_Instance, Hth, &Can_TxBuffCfg)  ← ⚠️ 关键！
  │      为什么每次发送前必须重配 TX MB? → 见下方"关键技巧"
  │
  ├── 3. 构造 can_message_t（MCAL 格式 → PAL 格式）
  │      tx_msg.id     = PduInfo->id
  │      tx_msg.length = PduInfo->length
  │      for i: tx_msg.data[i] = PduInfo->data[i]
  │
  └── 4. CAN_Send(&Can_Instance, Hth, &tx_msg) → 映射为 E_OK/E_NOT_OK
         内部 → FLEXCAN_DRV_Send() → 写 MB CS 寄存器 CODE=0xC → 硬件自动发送
         注意：此函数立即返回！实际发送由 FlexCAN 硬件异步完成
```

> **关键技巧：为什么每次 Can_Write 前必须重配 TX MB？**
>
> NXP PAL 的 `CAN_Send()` 内部会检查 MB 的状态码（CS 字段）。一次成功发送后，硬件将 MB 的 CS 置为 INACTIVE（非空闲），而非 READY。下次调用 `CAN_Send()` 时 PAL 发现 MB 不是 READY，返回 `STATUS_BUSY`。
>
> 解决：每次 `Can_Write()` 前调 `CAN_ConfigTxBuff()`，它会复位 CS 字段到 READY。这是本项目最隐蔽的一个坑，也是 CAN 驱动能稳定运行的保障。

**调用示例**：

```c
Can_PduType tx = {0};  // 全量初始化（is_extended = false, is_remote = false）
tx.id     = 0x123UL;
tx.length = 8U;
tx.data[0] = counter & 0xFF;
tx.data[1] = (counter >> 8) & 0xFF;
tx.data[2] = 0xAA;
tx.data[3] = 0x55;
// ...

Std_ReturnType ret = Can_Write(0, &tx);   // AUTOSAR: 只传 Hth，不传 Controller
if (ret != E_OK) {
    // 错误处理
}
```

---

### 5.3 Can_Read — 对标 AUTOSAR SWS_Can_00016

```c
status_t Can_Read(uint8_t Controller, uint8_t Hrh, Can_PduType *PduInfo);
```

> **AUTOSAR 规范定义**：从由 Hrh（Hardware Receive Handle）标识的 RX 硬件对象读取一个 CAN PDU。如果有新帧到达，数据被复制到 PduInfo 并返回 STATUS_SUCCESS；否则返回 STATUS_ERROR。

| 参数 | 含义 | 本项目实际值 |
|------|------|-------------|
| `Hrh` | RX MB 的**绝对索引** | 1（= num_tx + 0 = 第二个 MB） |
| `PduInfo` | 指向接收缓冲区 | 出参，数据被填充到此结构体 |

**关键理解**：Hrh 是绝对索引，不是相对索引。本项目 TX=1 个，所以 RX MB 从索引 1 开始。

```
MB 索引:  0(Hth=0)    1(Hrh=1)    2..15
         ┌──────┐   ┌──────┐   ┌──────────┐
         │  TX  │   │  RX  │   │   空     │
         │0x123 │   │0x100 │   │          │
         └──────┘   └──────┘   └──────────┘
```

**内部流程**：

```
Can_Read(Controller=0, Hrh=1, &rxPdu)
  │
  ├── 1. 参数校验（比 Can_Write 多一项：Hrh ≥ Can_TxCount?）
  │      确保只在 RX 区域（索引 ≥ num_tx）读取
  │
  ├── 2. CAN_Receive(&Can_Instance, Hrh, &rx_msg)
  │      内部: 读 MB CS 字段 → 有新帧? → 读 ID/长度/数据 → 清 MB 标志
  │      无新帧 → 直接返回 STATUS_ERROR
  │
  └── 3. 有数据 → 填充 Can_PduType
           PduInfo->id          = rx_msg.id
           PduInfo->length      = rx_msg.length
           PduInfo->is_extended = (rx_msg.cs & 0x1) ? true : false
           for i: PduInfo->data[i] = rx_msg.data[i]
```

**调用示例**（轮询模式）：

```c
Can_PduType rx = {0};
if (Can_Read(0, 1, &rx) == STATUS_SUCCESS) {
    // 有新帧！rx.id 是 CAN ID, rx.data[0..7] 是载荷
    // 注意：这是纯轮询模式，AUTOSAR 完整方案应该用中断 + Can_MainFunctionRead
}
```

---

### 5.4 Can_SetControllerMode — 对标 AUTOSAR SWS_Can_00098

```c
Std_ReturnType Can_SetControllerMode(Can_ControllerType Controller,
                                     Can_ControllerStateType Transition);
```

> **AUTOSAR 规范定义**：切换 CAN 控制器的运行模式。本实现为纯软件状态机——仅修改 `Can_State[]` 全局变量，不操作硬件寄存器（NXP PAL 未暴露 Freeze/Run API）。

状态流转：

```
Can_Init() → STOPPED → SetControllerMode(STARTED) → STARTED (可收发)
                           SetControllerMode(STOPPED) → STOPPED (禁止收发)
Can_DeInit() → UNINIT
```

---

### 5.5 Can_DeInit — 对标 AUTOSAR SWS_Can_00014

```c
Std_ReturnType Can_DeInit(void);
```

> **AUTOSAR 规范定义**：反初始化 CAN 控制器，释放所有硬件资源。之后可重新 Init。

内部：`CAN_Deinit(&Can_Instance)` → `Can_Initialized = false` → `Can_State = CAN_CS_UNINIT`。

---

## 6. 完整使用示例

以下是从启动到收发一帧的完整代码：

```c
#include "Can.h"

// 1. 定义 Mailbox + 配置（在 Can_Cfg.c 中，不在应用层）
static const Can_HardwareObject tx_mb[] = {{.id = 0x123UL}};
static const Can_HardwareObject rx_mb[] = {{.id = 0x100UL}};

const Can_ConfigType Can_Config = {
    .controller       = 0,
    .max_num_mb       = 16,
    .flexcan_mode     = CAN_MODE_NORMAL,
    .prop_seg         = 7,   .phase_seg1  = 4,   .phase_seg2 = 1,
    .pre_divider      = 0,   .r_jumpwidth = 1,
    .num_tx_mailboxes = 1,   .num_rx_mailboxes = 1,
    .tx_mailboxes     = tx_mb,
    .rx_mailboxes     = rx_mb,
};

void can_demo(void) {
    // 2. 初始化（由 EcuM 统一调度，应用层不直接调 Can_Init）
    EcuM_Init();  // 内部: Can_Init(&Can_Config) → Can_SetControllerMode() → CanIf_Init()

    // 4. 启动控制器
    (void)Can_SetControllerMode(0, CAN_CS_STARTED);

    // 5. 发送一帧 (AUTOSAR 标准签名: 只传 Hth)
    Can_PduType tx = {0};
    tx.id = 0x123UL;
    tx.length = 8U;
    tx.data[0] = 0x01; tx.data[1] = 0x02;  // ... 填充数据
    Can_Write(0, &tx);   // Hth=0, 不传 Controller

    // 6. 轮询接收
    Can_PduType rx = {0};
    if (Can_Read(0, 1, &rx) == STATUS_SUCCESS) {
        // 处理 rx.data[0..7]
    }

    // 7. 反初始化
    (void)Can_SetControllerMode(0, CAN_CS_STOPPED);
    (void)Can_DeInit();
}
```

---

## 7. 位时序：波特率怎么算

CAN 波特率 = 时钟源频率 / (pre_divider + 1) / (1 + prop_seg + phase_seg1 + phase_seg2)

本项目的计算：

```
时钟源:      PE 直连外部 8 MHz OSC (CAN_CLK_SOURCE_OSC)
pre_divider: 0 → 分频系数 = 1
TQ 总数:     1 (sync_seg, 固定) + 7 (prop_seg) + 4 (phase_seg1) + 1 (phase_seg2) = 13 TQ

波特率 = 8,000,000 / 1 / 13 ≈ 615,385 bps
```

不是精确的 500,000 bps，而是约 615 kbps。CAN 协议允许一定的波特率偏差（通常 ±1%~±3%），USB-CAN 适配器自动适应。

> 如果需要精确 500 kbps，可调整 `pre_divider` 或选择更高精度时钟源。但本项目已验证当前配置与 CANable (gs_usb) 正常通信。

---

## 8. 常见问题与调试

### 8.1 Can_Write 返回 STATUS_ERROR

| 可能原因 | 排查方法 |
|----------|---------|
| 没调 `Can_SetControllerMode(STARTED)` | 检查 `Can_State[0]` 是否为 `CAN_CS_STARTED` |
| `Hth` 超出 TX MB 范围 | 确认 `Hth < num_tx_mailboxes` |
| `PduInfo->length > 8` | CAN 经典帧最大 8 字节 |

### 8.2 接收不到数据

| 可能原因 | 排查方法 |
|----------|---------|
| RX MB 的过滤 ID 不匹配 | 检查 `rx_mailboxes[0].id` 是否等于发送方的 CAN ID |
| 没调 `Can_SetControllerMode(STARTED)` | 同 7.1 |
| CAN 总线物理断开 | 检查 CAN_H/CAN_L 接线和 120Ω 终端电阻 |
| 波特率不匹配 | 双方波特率必须一致（允许约 ±3% 偏差） |

### 8.3 Can_Write 第一次成功，第二次失败

这就是第 4.2 节说的 **TX MB 重配问题**——确认 `Can_Write()` 内部是否每次都调了 `CAN_ConfigTxBuff()`。

---

## 9. 文件清单

| 文件 | 说明 |
|------|------|
| `mcu/MCAL/Can/include/Can.h` | 类型定义 + API 声明（AUTOSAR 标准接口，不引用任何 NXP 头文件） |
| `mcu/MCAL/Can/src/Can.c` | 驱动实现（~200 行），封装 NXP CAN PAL |
| `mcu/MCAL/Can/config/Can_Cfg.h` | 配置常量 + `Can_SignalDefType`（信号级定义，后续迁移到 Com 层） |
| `mcu/MCAL/Can/config/can_pal_cfg.h` | PAL 层配置（FlexCAN 实例数等，NXP 工具生成） |

---

## 10. 与上层的关系

本文档是 MCAL 层的终点。向上连接：

```
Can (本文) ──→ CanIf (3_CanIf_CAN接口层详解.md) ──→ PduR ──→ Com ──→ RTE ──→ SWC
   ↑                ↑
 已实现           已实现                          待实现
```

调用关系：

- **Can_Write()** ← 当前被 `CanIf_Transmit()` 调用（[3_CanIf_CAN接口层详解.md](./3_CanIf_CAN接口层详解.md) 第 4.2 节）
- **Can_Read()** ← 当前在 `main.c` 中轮询，结果转发给 `CanIf_RxIndication()`
- **Can_Init()** ← 已迁移到 `EcuM_Init()` 中统一调度，配置数据在 `Can_Cfg.c` 中定义（`extern const Can_ConfigType Can_Config`）
