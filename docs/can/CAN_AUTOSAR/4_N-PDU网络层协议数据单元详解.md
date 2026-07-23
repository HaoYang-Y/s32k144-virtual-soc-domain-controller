# N-PDU — 网络层协议数据单元详解

> 从零开始，讲清楚 AUTOSAR 中最容易被误解的概念：N-PDU 到底是什么、它和 I-PDU/L-PDU 的区别、为什么 AUTOSAR 不为它定义独立的类型，以及 CAN TP 帧如何在 CanTp ↔ CanIf 之间以 N-PDU 的身份流转。

---

## 阅读前你需要知道

| 前置知识 | 文档 |
|----------|------|
| AUTOSAR 五层全景（CanTp/CanIf 在整个架构中的位置） | [1_AUTOSAR_CP_CAN通信栈.md](./1_AUTOSAR_CP_CAN通信栈.md) |
| CanIf 层（PDU ID 概念、PDU→CAN ID 查表） | [3_CanIf_CAN接口层详解.md](./3_CanIf_CAN接口层详解.md) |

> 不需要预先知道 CAN TP 协议——本文 §3 会从零介绍 PCI 和四种帧类型。它们的完整交互逻辑（FC 流控、状态机）在 [第 5 篇](./5_CanTp_CAN传输层详解.md) 展开。

---

## 1. 先看一个让你困惑的现象

在 AUTOSAR 的文档和代码里，你可能会同时看到这三个词：

```
I-PDU   (Interaction Layer PDU)
N-PDU   (Network PDU)
L-PDU   (Data Link Layer PDU)
```

**直觉反应**："哦，AUTOSAR 定义了三种 PDU 类型，分别对应三个层级。"

**实际情况**：大错特错。AUTOSAR 从头到尾**只定义了一个 PDU 数据类型**。I-PDU、N-PDU、L-PDU 是**同一个东西在不同层级时的"角色名"**，而不是三个不同的类型。

用一句话概括：

> **N-PDU 不是一种数据类型，而是一个角色。** 它是 `PduInfoType` 在 CanTp ↔ CanIf 之间流转时的名字。

---

## 2. 什么是 N-PDU？

### 2.1 一个数据包的三个人称

想象一个快递包裹从北京发往上海：

| 阶段 | 谁在看它 | 叫它什么 |
|------|---------|---------|
| 寄件人打包 | 寄件人 | "包裹" |
| 快递中转站分拣 | 快递员 | "件" |
| 送到收件人手里 | 收件人 | "快递" |

**包裹本身没变**，但在不同人眼里有不同叫法。

AUTOSAR 中完全一样：

```
Com ──→ PduR ──→ CanTp ──→ CanIf ──→ Can (MCAL) ──→ CAN 总线
  I-PDU    I-PDU    N-PDU    N-PDU     L-PDU
```

| 层间流转 | 叫法 | 原因 |
|----------|------|------|
| Com ↔ PduR | **I-PDU** | Interaction Layer（交互层），含多个信号的打包数据 |
| PduR ↔ CanTp | **I-PDU** | 同上，经过了路由器 |
| **CanTp ↔ CanIf** | **N-PDU** | Network Layer（网络层），CAN TP 帧 |
| CanIf ↔ Can (MCAL) | N-PDU → L-PDU 转换 | Data Link Layer，变成硬件帧格式 |

**传送的 `PduInfoType` 结构体是同一个**，区别只在于：
- 当它从 Com 传来时，我们叫它 I-PDU（交互层视角）
- 当它从 CanTp 传给 CanIf 时，我们叫它 N-PDU（网络层视角）
- 当 CanIf 把它转成 `Can_PduType` 交给 Can 驱动时，那个叫 L-PDU

### 2.2 类型定义：全栈唯一

AUTOSAR 在 `ComStack_Types.h` 中定义了**唯一的** PDU 交换类型：

```c
// ===== mcu/include/ComStack_Types.h =====
// 对标 AUTOSAR SWS_ComStackTypes

typedef uint16_t PduIdType;        // PDU ID（全局）
typedef uint16_t PduLengthType;    // PDU 长度

typedef struct {
    PduIdType     SduId;           // PDU ID（AUTOSAR 标准字段名）
    PduLengthType SduLength;       // 数据长度
    uint8_t      *SduDataPtr;      // 指向数据的指针
} PduInfoType;
```

**关键事实**：

- ❌ AUTOSAR 没有 `NPduType`
- ❌ AUTOSAR 没有 `IPduType`
- ❌ AUTOSAR 没有 `LPduType`
- ✅ AUTOSAR 只有 `PduInfoType`，所有模块共用

CanTp、CanIf、PduR、Com 四个模块的 API 全部使用 `PduInfoType*`：

```c
// CanIf — AUTOSAR SWS_CanIf_00221
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

// CanTp — AUTOSAR SWS_CanTp_00020
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

// PduR — AUTOSAR SWS_PduR
Std_ReturnType PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
```

> 💡 **AUTOSAR 规范依据**：`SWS_CanIf_00005`、`SWS_CanTp_00005`、`SWS_PduR_00005` 都明确要求使用 `ComStack_Types.h` 中的 `PduInfoType`。模块**不能**自己定义另一套 PDU 数据结构。

---

## 3. N-PDU 的"肉身"：CAN TP 帧格式

当 `PduInfoType` 以 N-PDU 的身份在 CanTp ↔ CanIf 之间传输时，它承载的是 **CAN TP 帧**（ISO 15765-2）。

### 3.1 PCI 字节：帧的"身份证"

每个 CAN TP 帧的第一个字节叫 **PCI（Protocol Control Information，协议控制信息）**。它的高 4 位决定了帧类型：

```
PCI 字节 = [Type:4bit] [Info:4bit]

  Type = 0 (0000) → Single Frame (SF)     单帧
  Type = 1 (0001) → First Frame (FF)      首帧
  Type = 2 (0010) → Consecutive Frame (CF) 连续帧
  Type = 3 (0011) → Flow Control (FC)     流控帧
```

### 3.2 四种帧的编码规则

**SF（Single Frame）— 数据 ≤ 7 字节，一帧搞定**

```
CAN Data: [0x0N] [D0] [D1] ... [DN] [padding...]
              ↑
         PCI: 高4bit=0, 低4bit=N=数据长度(1~7)

示例: 发 3 字节数据 {0xAA, 0xBB, 0xCC}
CAN Data: 03 AA BB CC 00 00 00 00
          ↑  ↑──┬──↑
        PCI=0x03  payload=3 bytes
```

**FF（First Frame）— 数据 > 7 字节，第一条帧声明总长度**

```
CAN Data: [0x1N] [NH] [D0] [D1] ... [D5]
              ↑    ↑
    PCI byte0: 高4bit=1, 低4bit=总长度[11:8]
    PCI byte1: 总长度[7:0]
    后面 6 字节 = payload 的前 6 字节

示例: 发 20 字节数据, 总长度=0x0014
CAN Data: 10 14 AA BB CC DD EE FF 00
          ↑  ↑  ↑──────────┬─────────↑
      PCI[0] PCI[1]   payload前6字节
```

**CF（Consecutive Frame）— FF 之后的数据帧，7 字节 payload**

```
CAN Data: [0x2N] [D0] ... [D6]
              ↑
         PCI: 高4bit=2, 低4bit=序号(0~15, 从1开始, 回绕)

示例: 第 1 个 CF (序号=1), 数据=AABBCCDDEEFF00
CAN Data: 21 AA BB CC DD EE FF 00
```

**FC（Flow Control）— 接收方控制发送方节奏**

```
CAN Data: [0x30] [BS] [STmin] [padding...]
              ↑    ↑     ↑
         PCI=0x30 块大小  最小间隔(100us单位)

示例: BS=8(每8帧确认), STmin=1(100us间隔)
CAN Data: 30 08 01 AA AA AA AA AA
```

### 3.3 PCI 编解码速查

```c
// 编码: 根据数据长度构造 PCI
uint8_t CanTp_EncodeSF(uint8_t len)   // → 0x0N
void    CanTp_EncodeFF(uint16_t total, uint8_t pci[2])  // → {0x1N, NH}
uint8_t CanTp_EncodeCF(uint8_t seq)   // → 0x2N
void    CanTp_EncodeFC(uint8_t bs, uint8_t stmin, uint8_t pci[3])  // → {0x30, BS, STmin}

// 解码: 从 CAN 数据首字节提取帧类型
CanTp_FrameType CanTp_DecodePci(data, &sfLen, &ffTotalLen, &cfSeqNum);
```

---

## 4. N-PDU 的"旅程"：一次完整的多帧传输

用一个具体例子走通 N-PDU 从创建、发送、到接收重组的**完整生命旅程**。

### 4.1 场景设定

上层（Com）要发送一个 20 字节的 I-PDU，目标 CAN ID = `0x123`。

```
20 字节 I-PDU = "HELLO AUTOSAR CAN TP!" (20 chars)
```

### 4.2 TX 路径：I-PDU → 多个 N-PDU

```
        Com
         │ I-PDU (20 bytes 完整消息)
         ▼
       PduR
         │ PduInfoType {SduId=0, SduLength=20, SduDataPtr→"HELLO...""}
         ▼
       CanTp ─── 分段逻辑 ───┐
         │                    │
         │  ┌─────────────────┘
         │  │
         │  ├─ N-PDU #1 (FF): 声明总长度 + 前6字节
         │  │   PduInfoType {SduId=0, SduLength=8, SduDataPtr→[10 14 'H' 'E' 'L' 'L' 'O' ' ']}
         │  │   PCI=0x10 14 → FF, totalLen=0x14=20, payload="HELLO "
         │  │   → CanIf_Transmit(cfg->canif_pdu_id, &npdu1)
         │  │
         │  ├─ 等待 FC（流控帧）← 从 RX 方向回来
         │  │
         │  ├─ N-PDU #2 (CF seq=1): 第 2 段 7 字节
         │  │   PduInfoType {SduId=0, SduLength=8, SduDataPtr→[21 'A' 'U' 'T' 'O' 'S' 'A' 'R']}
         │  │   PCI=0x21 → CF, seq=1, payload="AUTOSAR"
         │  │   → CanIf_Transmit(cfg->canif_pdu_id, &npdu2)
         │  │
         │  └─ N-PDU #3 (CF seq=2): 最后 7 字节
         │      PduInfoType {SduId=0, SduLength=8, SduDataPtr→[22 ' ' 'C' 'A' 'N' ' ' 'T' 'P']}
         │      PCI=0x22 → CF, seq=2, payload=" CAN TP"
         │      → CanIf_Transmit(cfg->canif_pdu_id, &npdu3)
         │
         ▼
       CanIf ─── Pdu ID → CAN ID 查表 ───┐
         │                                │
         │   N-PDU → L-PDU 格式转换:
         │   CanIf_PduConfig[0] → can_id=0x123
         │
         ├─ Can_PduType {id=0x123, data=10 14 48 45 4C 4C 4F 20}
         │  → Can_Write(HTH=0, &canPdu)
         │
         ├─ Can_PduType {id=0x123, data=21 41 55 54 4F 53 41 52}
         │  → Can_Write(HTH=0, &canPdu)
         │
         └─ Can_PduType {id=0x123, data=22 20 43 41 4E 20 54 50}
            → Can_Write(HTH=0, &canPdu)
              │
              ▼
         CAN 总线: ID=0x123, 三个 CAN 帧依次发出
```

### 4.3 RX 路径：N-PDU → I-PDU 重组

```
CAN 总线 ──→ Can (MCAL)
              │ Can_PduType (CAN 帧)
              ▼
            CanIf
              │ PduInfoType → CanTp_RxIndication
              ▼
            CanTp ─── 重组逻辑 ───┐
              │                   │
              ├─ RX N-PDU #1 (FF): PCI=0x10 14
              │   → 解析: totalLen=20, payload_start="HELLO "
              │   → 分配 20 字节缓冲区
              │   → 发送 FC: PCI=0x30 08 01
              │
              ├─ RX N-PDU #2 (CF seq=1): PCI=0x21
              │   → payload="AUTOSAR" → 追加到缓冲区 offset=6
              │
              └─ RX N-PDU #3 (CF seq=2): PCI=0x22
                  → payload=" CAN TP" → 追加到缓冲区 offset=13
                  → rx_index=20 = totalLen → 重组完成!
              │
              ▼
            PduR_CanIfRxIndication()
              │ PduInfoType {SduId=0, SduLength=20, SduDataPtr→"HELLO AUTOSAR CAN TP!"}
              ▼
            Com → 拆出信号 → SWC
```

---

## 5. N-PDU 的三个关键规则

### 5.1 规则一：N-PDU 就是 PduInfoType

不要在代码里创建 `NPduType`、`CanTp_NPduType` 之类的东西。AUTOSAR 用 `PduInfoType` 统一表示所有跨层 PDU。**这是 AUTOSAR 最重要的设计决策之一**——用同一个类型加 PDU ID 路由，避免层层类型转换。

### 5.2 规则二：N-PDU 的大小 ≤ 8 字节（经典 CAN）

| 帧类型 | CAN 帧 payload | N-PDU 数据量 |
|--------|---------------|-------------|
| SF | 1(PCI) + 1~7 = 2~8 字节 | 1~7 字节有效数据 |
| FF | 2(PCI) + 6 = 8 字节 | 6 字节 + 总长度声明 |
| CF | 1(PCI) + 7 = 8 字节 | 7 字节有效数据 |
| FC | 3(PCI) = 3 字节 | 0 字节（纯控制） |

**每个 N-PDU 恰好填满一个 CAN 帧的 8 字节 data 字段。** 发送时即使不到 8 字节也填充到 8 字节（DLC=8），接收方按 PCI 指示的有效长度提取数据。

### 5.3 规则三：N-PDU 的 PDU ID 来自 CanIf_PduConfig

N-PDU 的 `SduId`（即 PDU ID）必须与 `CanIf_PduConfig` 表中的某个条目对应。CanIf 通过这个 ID 查到 CAN ID 和控制器：

```c
// CanIf_PduConfig[] — PDU ID 到 CAN ID 的映射表
{ .pdu_id = 0, .can_id = 0x123, .controller_id = 0, .dlc = 8 }  // TX PDU
{ .pdu_id = 1, .can_id = 0x100, .controller_id = 0, .dlc = 8 }  // RX PDU

// CanTp 通过 canif_pdu_id 告诉 CanIf "用哪个 PDU 发这个 N-PDU"
CanIf_Transmit(cfg->canif_pdu_id, &nPduInfo);
```

---

## 6. 本项目的 N-PDU 实现

### 6.1 文件位置

| 内容 | 位置 | AUTOSAR 依据 |
|------|------|-------------|
| `PduInfoType` 定义 | `mcu/include/ComStack_Types.h` | SWS_ComStackTypes |
| N-PDU 编解码 + 状态机 | `mcu/Services/CanTp/src/CanTp.c` | SWS_CanTp |
| N-PDU 通道配置 | `mcu/Services/CanTp/config/CanTp_Cfg.h` | — |
| N-PDU → L-PDU 转换 | `mcu/EcuAbstraction/CanIf/src/CanIf.c` | SWS_CanIf |

### 6.2 核心 API（当前已实现）

```c
// ===== CanTp =====
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
//  输入: I-PDU (完整消息)
//  输出: 1~N 次 CanIf_Transmit() 调用 (每次发一个 N-PDU)

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
//  输入: N-PDU (一个 CAN TP 帧)
//  输出: 收齐后调用 PduR_CanIfRxIndication() (提交完整 I-PDU)

// ===== CanIf =====
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
//  输入: N-PDU
//  输出: Can_Write() — N-PDU → L-PDU 格式转换后发送

void CanIf_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
//  输入: 从 CAN 帧还原的 N-PDU
//  输出: 转发给 CanTp 或 PduR
```

### 6.3 当前限制与后续规划

| 功能 | 状态 | 说明 |
|------|------|------|
| SF 单帧收发 | ✅ 已实现 | ≤7 字节，一帧搞定 |
| FF+CF 多帧发送 | ✅ 已实现 | 简化模式：顺序发送，不等待 FC |
| FF+CF 多帧接收 | ✅ 已实现 | 含 FC 回复和缓冲区重组 |
| FC 流控状态机 | ⚠️ 简化 | 发送方不等待 FC 直接发完；接收方会回复 FC |
| 超时处理 | ⚠️ 骨架 | N_As/N_Ar/N_Bs/N_Cr 参数已定义，定时器逻辑待实现 |
| 多通道并发 | ✅ 已支持 | 当前配置 2 个通道（TX + RX） |

### 6.4 简单示例：发一条 5 字节消息（走 SF 单帧）

```c
#include "ComStack_Types.h"
#include "CanIf.h"
#include "CanIf_PduId.h"

// 1. 准备数据
uint8_t myData[5] = {0x01, 0x02, 0x03, 0x04, 0x05};

// 2. 构建 PduInfoType（它此刻就是 N-PDU）
PduInfoType myNPdu = {
    .SduId      = CANIF_PDU_ID_TX_0x123,   // N-PDU 的 PDU ID
    .SduLength  = 5U,                       // 5 字节数据
    .SduDataPtr = myData,                   // 指向数据
};

// 3. 发送 N-PDU 到 CanIf
//    由于 5 ≤ 7，走 SF 单帧，PCI 字节 = 0x05
//    CAN 总线上实际帧: ID=0x123, data=[05 01 02 03 04 05 00 00]
Std_ReturnType ret = CanIf_Transmit(CANIF_PDU_ID_TX_0x123, &myNPdu);
// → E_OK
```

---

## 7. N-PDU vs I-PDU vs L-PDU：一张表说清楚

| | I-PDU | N-PDU | L-PDU |
|--|-------|-------|-------|
| **全称** | Interaction Layer PDU | Network PDU | Data Link Layer PDU |
| **流转层级** | Com ↔ PduR ↔ CanTp | **CanTp ↔ CanIf** | CanIf ↔ Can (MCAL) |
| **数据类型** | `PduInfoType` | **`PduInfoType`**（同一个！） | `Can_PduType`（不同！） |
| **数据指针** | `SduDataPtr` → 完整消息 | `SduDataPtr` → PCI + 分段数据 | `data[8]` → 定长 8 字节数组 |
| **大小** | 任意（受 TP 限制） | ≤ 8 字节（经典 CAN） | **恰好 8 字节**（DLC=8） |
| **ID 含义** | PDU 路由 ID | PDU 路由 ID（→ CanIf 查表得 CAN ID） | CAN 报文 ID（硬件地址） |
| **PCI 字节** | 无 | **有**（SF/FF/CF/FC） | 无 |
| **创建者** | Com 模块 | CanTp（TX）/ CanIf→CanTp（RX） | CanIf（从 N-PDU 转换来） |
| **AUTOSAR 类型定义** | `ComStack_Types.h` | `ComStack_Types.h` | `Can.h` |

> **唯一需要类型转换的地方**：CanIf 内部 N-PDU（`PduInfoType*`）→ L-PDU（`Can_PduType`）。其他所有层之间都是**同一个 `PduInfoType` 指针直接传递**，零开销。

---

## 8. 常见误解 FAQ

### Q1: "N-PDU 是不是就是 CAN 帧的数据部分？"

**不完全是。** N-PDU 包含了 PCI 字节 + 数据，填满了一个 CAN 帧的 8 字节 data 区域。但 N-PDU 本身是 `PduInfoType` 结构体（含 PDU ID + 长度 + 数据指针），它还不是 `Can_PduType`。CanIf 负责把 `PduInfoType`（N-PDU）转换成 `Can_PduType`（L-PDU）——这个转换主要就是 PDU ID → CAN ID 的查表翻译。

### Q2: "是不是应该定义一个 NPduType？"

**不应该。** AUTOSAR 特意不这么做。如果每个层都定义自己的 PDU 类型，每经过一层就要做类型转换（I-PDU → N-PDU → L-PDU），代码又臭又慢。AUTOSAR 的设计是：一个 `PduInfoType` 贯穿全栈，区分靠的是**数据当前所在的层**和**PDU ID 的路由配置**。

### Q3: "CanTp 发给 CanIf 的到底是 PduInfoType 还是 Can_PduType？"

**`PduInfoType`（即 N-PDU）**。CanIf 收到后再内部转换为 `Can_PduType`。`Can_PduType` 只在 MCAL 边界使用，上层从来看不到它。

### Q4: "为什么 I-PDU 和 N-PDU 用同一个类型，L-PDU 却不一样？"

因为 I-PDU 和 N-PDU 都是**逻辑层**的数据——它们用指针指向内存中的缓冲区，大小不固定。而 L-PDU（`Can_PduType`）是**硬件层**的数据——它对应 CAN 控制器的硬件 Mailbox，必须是定长 8 字节数组，且要填写 CAN ID、帧格式等硬件参数。这是软件抽象和硬件之间的天然边界。

---

## 9. 一句话总结

> **N-PDU 是 `PduInfoType` 在 CanTp ↔ CanIf 之间流转时的角色名。它不拥有独立的类型定义，而是通过 PCI 字节（CAN TP 帧头）在同一个 `PduInfoType` 结构体上叠加了网络层的语义——分段、重组、流控。**

把它记住，你就理解了 AUTOSAR COM Stack 类型体系中最重要的设计决策。

---

## 10. 相关文件

| 文件 | 说明 |
|------|------|
| `mcu/include/ComStack_Types.h` | `PduInfoType` 定义（全栈唯一 PDU 类型） |
| `mcu/Services/CanTp/include/CanTp.h` | CanTp API + PCI 编解码函数声明 |
| `mcu/Services/CanTp/src/CanTp.c` | N-PDU 创建/消费逻辑（SF/FF/CF/FC 状态机） |
| `mcu/Services/CanTp/config/CanTp_Cfg.h` | N-PDU 通道配置（TP PDU → CanIf PDU 映射） |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | N-PDU → L-PDU 格式转换 |
