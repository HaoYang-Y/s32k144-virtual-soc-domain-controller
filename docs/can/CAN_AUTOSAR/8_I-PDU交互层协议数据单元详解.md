# I-PDU — 交互层协议数据单元详解

> AUTOSAR CAN 通信栈系列第 8 篇。I-PDU 是三种 PDU 中最"高级"的一种——它是 SWC 信号的容器，不含任何协议头，是 Com 层直接操作的数据单元。

---

## 1. I-PDU 在三层 PDU 中的位置

```
信号 (Signal) ──→ I-PDU ──→ N-PDU ──→ L-PDU ──→ 物理总线
  ↑               ↑         ↑         ↑
 Com 层          PduR     CanTp     Can/CanIf
```

| | L-PDU (3) | N-PDU (5) | I-PDU (8) |
|---|---|---|---|
| **在哪两层之间** | CanIf ↔ Can | CanTp ↔ CanIf | Com ↔ PduR ↔ CanTp |
| **数据类型** | `Can_PduType` | `PduInfoType` | `PduInfoType` |
| **有无协议头** | 无（裸 CAN 帧） | 有（PCI 分包字节） | **无**（纯信号数据） |
| **数据长度** | 定长 8 字节 | 可变（含 PCI） | 可变（信号总和） |
| **谁产生/消费** | CanIf | CanTp | **Com** |

**关键**：I-PDU 是 CanTp 去掉 PCI 之后、Com 还没解包成信号之前的"中间态"——它是信号的集装箱，多个信号打包在一起。

---

## 2. I-PDU 长什么样？

以本项目的 I-PDU 0x123（TX）为例：

```
I-PDU 0x123 (DLC=6, 周期 500ms):
┌────────────┬──────────┬──────────┐
│ byte 0-3   │ byte 4   │ byte 5   │
│ Counter    │ Magic0   │ Magic1   │
│ 32-bit     │ 8-bit    │ 8-bit    │
│ Intel LE   │ Intel LE │ Intel LE │
└────────────┴──────────┴──────────┘

在内存中（PduInfoType）:
  SduId      = 0 (I-PDU ID)
  SduLength  = 6 (6 字节)
  SduDataPtr → [05][00][00][00][AA][55]  ← 纯信号数据
```

注意：I-PDU 的数据区**不含任何协议头**。PCI（SF/FF/CF/FC）是 N-PDU 层的概念，L-PDU 的 `Can_PduType` 有 CAN ID + DLC 等硬件字段——这些在 I-PDU 层都不存在。

---

## 3. I-PDU 的一生——从 Com 到 CAN 总线

以一次实际发送为例（Counter=0x05, Magic0=0xAA, Magic1=0x55）：

```
1. Com 打包信号:
   Com_SendSignal 三次 →
   Com_PackSignal 把三个信号写入 I-PDU buffer
   I-PDU = {0x05, 0x00, 0x00, 0x00, 0xAA, 0x55} (6 bytes)

2. Com_MainFunction 触发发送:
   PduR_ComTransmit(PduId=0, I-PDU=6 bytes)  ← ★ 此时还是 I-PDU

3. PduR 转发:
   CanTp_Transmit(PduId=0, I-PDU=6 bytes)    ← ★ PduR 眼中还是 I-PDU

4. CanTp 加 PCI:
   "6 ≤ 7 → SF 单帧"
   加 PCI 字节 0x06 → N-PDU = {0x06, 0x05, 0x00, 0x00, 0x00, 0xAA, 0x55} (7 bytes)
   ★ 这一刻，I-PDU 变成了 N-PDU

5. CanIf 转 L-PDU:
   Can_PduType{id=0x123, len=8, data={0x06, 0x05, 0x00, 0x00, 0x00, 0xAA, 0x55, 0x00}}
   ★ 这一刻，N-PDU 变成了 L-PDU

6. CAN 总线:
   candump 看到: can0 123 [8] 06 05 00 00 00 AA 55 00
```

**I-PDU 的"寿命"很短**——从 Com 打包完到 CanTp 加 PCI 之前，仅在 Com→PduR→CanTp 这一段路上存在。

---

## 4. I-PDU 在接收路径上

反过来，收帧时 I-PDU 是 CanTp 重组完的产物：

```
CAN 总线 → Can → CanIf → CanTp_RxIndication:
  ├─ PCI 解码: 0x07 = SF(7 字节数据)
  ├─ 去 PCI: data[1..7] = AA BB CC DD EE FF 00
  └─ PduR_CanTpRxIndication(PduId=1, I-PDU=7 bytes)
       │
       ▼  ★ 这一刻起，数据身份从 N-PDU 变回 I-PDU
     Com_RxIndication(PduId=1, I-PDU=7 bytes)
       │
       ├─ Com_UnpackSignal → TestRxData = 0x00FFEEDDCCBBAA
       └─ Update Bit = 1, SignalStatus = OK
```

---

## 5. I-PDU 的配置

每个 I-PDU 在 Com_Cfg.h 中有一行配置：

```c
typedef struct {
    Com_IPduIdType  ipdu_id;        // I-PDU ID (0, 1, ...)
    Com_IPduIdType  group_id;       // 所属 IPduGroup
    uint8_t         dlc;            // 数据长度 (1~8 字节)
    uint32_t        can_id;         // 目标 CAN ID
    uint16_t        cycle_time_ms;  // 周期发送间隔 (0=事件触发)
} Com_IPduConfigType;
```

本项目配置了 2 个 I-PDU：

| I-PDU | CAN ID | 方向 | DLC | 周期 | 包含信号 |
|-------|--------|------|-----|------|----------|
| 0 | 0x123 | TX | 6 | 500ms | Counter(32b) + Magic0(8b) + Magic1(8b) |
| 1 | 0x100 | RX | 7 | 0(事件) | RxData(64b) |

**多个信号共享一个 I-PDU = 共享一条 CAN 帧。**

---

## 6. I-PDU 和 Signal（信号）的关系

这是最容易搞混的。一句话：

> **I-PDU 是集装箱，Signal 是里面的包裹。**

```
        I-PDU 0x123 (CAN ID 0x123)
       ┌────────────────────────────┐
       │ ┌──────────┐ ┌───┐ ┌───┐  │
       │ │ Counter  │ │ M0│ │ M1│  │
       │ │ 32-bit   │ │ 8b│ │ 8b│  │
       │ └──────────┘ └───┘ └───┘  │
       └────────────────────────────┘
         ↑                    ↑
      三个独立的 Signal   共享一个 I-PDU
```

SWC 不操作 I-PDU——它只通过 `Com_SendSignal(SignalId, &value)` 写信号值。信号的位布局（在 I-PDU 的哪个位置、占多少位）由配置表决定：

```c
// Com_Cfg.c — 信号在 I-PDU 中的位布局
{
    .signal_id    = COM_SIGNAL_ID_TEST_TX_COUNTER,
    .ipdu_id      = COM_IPDU_ID_TX_0x123,  // ← 属于哪个 I-PDU
    .bit_position = 0U,                     // ← 在 I-PDU 中的起始位
    .bit_size     = 32U,                    // ← 占多少位
    .byte_order   = COM_BYTE_ORDER_INTEL,
},
```

---

## 7. I-PDU 的运行时状态

I-PDU 在内存中有对应的运行时 buffer：

```c
typedef struct {
    uint8_t  data[8];        // I-PDU 字节数组 ← 集装箱本体
    uint32_t last_tx_ms;     // 上次发送时间（周期判断用）
    uint8_t  dirty;          // 脏标记: 有信号更新了但还没发
    uint8_t  enabled;        // IPduGroup 是否启用
} Com_IPduBufferType;
```

`dirty` 标志是 COM 层的核心优化：同一 I-PDU 上多个信号更新后，只发一次 CAN 帧。

---

## 8. 三种 PDU 总结

```
         Com 视角          PduR/CanTp 视角       Can/CanIf 视角
      ┌──────────┐      ┌──────────────┐      ┌──────────────┐
      │ I-PDU    │ +PCI │ N-PDU        │ 定长 │ L-PDU        │
      │ 信号容器  │ ───→ │ 含分包头      │ ───→ │ CAN 硬件帧   │
      │ 无协议头  │      │ SF/FF/CF/FC  │      │ data[8] 定长 │
      └──────────┘      └──────────────┘      └──────────────┘
        Com 操作           CanTp 操作            CanIf/Can 操作
```

| 文档 | PDU 类型 | 在哪层 |
|------|----------|--------|
| [3_L-PDU](./3_L-PDU数据链路层协议数据单元详解.md) | L-PDU | CanIf ↔ Can |
| [5_N-PDU](./5_N-PDU网络层协议数据单元详解.md) | N-PDU | CanTp ↔ CanIf |
| [本文](./8_I-PDU交互层协议数据单元详解.md) | I-PDU | Com ↔ PduR ↔ CanTp |

---

## 参考文件

| 文件 | 内容 |
|------|------|
| `mcu/Services/Com/config/Com_Cfg.h` | I-PDU 配置结构 (`Com_IPduConfigType`) |
| `mcu/Services/Com/config/Com_Cfg.c` | I-PDU 配置实例 (2 个 I-PDU) |
| `mcu/Services/Com/src/Com.c` | I-PDU 缓冲区管理 (`Com_IPduBufferType`) |
| `mcu/include/ComStack_Types.h` | `PduInfoType` — I-PDU/N-PDU 共用的数据类型 |
