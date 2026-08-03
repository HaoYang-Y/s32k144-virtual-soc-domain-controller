# L-PDU — 数据链路层协议数据单元详解

> AUTOSAR CAN 通信栈系列。L-PDU 是三种 PDU（I-PDU / N-PDU / L-PDU）中唯一有**独立数据类型**的——Can_PduType。
> 本文讲解 L-PDU 的定义、为什么是定长 8 字节、以及 CanIf 如何将 N-PDU 转换为 L-PDU。

---

## 1. L-PDU 是什么？

L-PDU（Data Link Layer PDU）就是 **CAN 硬件帧**。它是在 CanIf 层和 MCAL Can 驱动之间传递的数据单元。

和 I-PDU / N-PDU 不同，L-PDU **有自己独立的数据类型**，不用 `PduInfoType`：

```c
// mcu/MCAL/Can/include/Can.h
typedef struct {
    Can_IdType   id;          // CAN 报文 ID (11-bit 标准帧 或 29-bit 扩展帧)
    uint8_t      length;      // 数据长度 DLC (0~8)
    bool         is_extended; // 是否扩展帧
    bool         is_remote;   // 是否远程帧
    uint8_t      data[8];     // ★ 数据载荷 — 定长 8 字节
} Can_PduType;
```

**为什么 I-PDU / N-PDU 可以共用 `PduInfoType`，而 L-PDU 必须用 `Can_PduType`？**

因为到了硬件层，数据不再是"指针+长度"的形式——CAN 控制器需要知道 CAN ID、DLC、以及**固定 8 字节**的载荷。`Can_PduType` 完美映射到 FlexCAN 硬件 Mailbox 的寄存器布局。

---

## 2. 三种 PDU 对比

| | I-PDU | N-PDU | L-PDU |
|---|---|---|---|
| **全称** | Interaction Layer PDU | Network PDU | Data Link Layer PDU |
| **数据类型** | `PduInfoType` | `PduInfoType` | **`Can_PduType`** |
| **数据结构** | 指针 + 可变长 | 指针 + 可变长 + PCI 头 | **定长 8 字节数组** |
| **在哪层** | Com ↔ PduR | CanTp ↔ CanIf | CanIf ↔ Can (MCAL) |
| **特点** | 纯信号数据，无协议头 | 加了 PCI 分包字节 | CAN 2.0 物理帧格式 |

**关键认知**：I-PDU 和 N-PDU 是**同一个 `PduInfoType`** 在不同层的"角色名"。L-PDU 则是**不同的数据类型**——因为硬件要求固定格式。

---

## 3. L-PDU 是怎么产生的？

L-PDU 由 CanIf 层产生。当上层（CanTp 或 PduR）传来一个 N-PDU 时，CanIf 做两件事：

1. **查表**：根据 PDU ID 找到对应的 CAN ID 和发送硬件句柄（HTH）
2. **格式转换**：把 `PduInfoType`（可变长指针）转成 `Can_PduType`（定长 8 字节数组）

```c
// mcu/EcuAbstraction/CanIf/src/CanIf.c
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfo)
{
    const CanIf_PduConfigType *cfg = CanIf_FindConfigByPduId(TxPduId);
    if (cfg == NULL) return E_NOT_OK;

    // ★ N-PDU → L-PDU 格式转换
    Can_PduType canPdu;
    canPdu.id          = cfg->can_id;           // 从配置表取 CAN ID
    canPdu.length      = PduInfo->SduLength;    // 数据长度 = DLC
    canPdu.is_extended = false;                 // 标准帧 (11-bit ID)
    canPdu.is_remote   = false;                 // 数据帧
    memcpy(canPdu.data, PduInfo->SduDataPtr, PduInfo->SduLength);

    return Can_Write(cfg->hth, &canPdu);        // L-PDU → MCAL 驱动
}
```

配置表（`CanIf_Cfg.c`）预先定义了每个 PDU ID 对应的 CAN ID 和 HTH：

```c
const CanIf_PduConfigType CanIf_PduConfig[CANIF_PDU_COUNT] = {
    {CANIF_PDU_ID_TX_0x123, 0U, 0x123, 8U, CAN_HTH_MAKE(0U, 0U)},  // TX: 0x123
    {CANIF_PDU_ID_RX_0x100, 0U, 0x100, 8U, 0U},                    // RX: 0x100
};
```

---

## 4. 一个完整的例子

以我们实测通过的数据为例，跟踪一个数据包从 Com 到 CAN 总线的完整变身：

```
Com 调用 Com_SendSignal(Counter=0x05, Magic0=0xAA, Magic1=0x55):
  写入 I-PDU buffer → 6 字节数据: 05 00 00 00 AA 55

Com_MainFunction 触发发送 → PduR_ComTransmit(PduId=0, I-PDU=6 bytes)

PduR 转发 → CanTp_Transmit(PduId=0, I-PDU=6 bytes)

CanTp: "6 ≤ 7，走 SF 单帧"
  加 PCI 头 0x06 → N-PDU = 06 05 00 00 00 AA 55 (7 bytes)
  CanTp_SendNPdu → CanIf_Transmit(PduId=0, N-PDU=7 bytes)

CanIf: "PDU 0 → CAN ID 0x123, HTH=(Controller=0, MB=0)"
  ★ N-PDU → L-PDU:
    Can_PduType {
      id          = 0x00000123,
      length      = 8,                ← CAN 帧 DLC=8
      is_extended = false,
      is_remote   = false,
      data[8]     = {06, 05, 00, 00, 00, AA, 55, 00}
                                           ↑ 第 8 字节 00 是填充
    }

Can_Write(HTH, &l_pdu):
  → CAN_ConfigTxBuff() + CAN_Send()
  → FlexCAN 硬件发出物理帧

candump 看到: can0 123 [8] 06 05 00 00 00 AA 55 00
                   ↑ID  ↑DLC  ↑ data[0]~data[7]
```

**注意**：CAN 帧的 DLC=8（`[8]`），但实际只有 7 字节有效数据（SF PCI 0x06 + 6 字节 I-PDU）。第 8 字节 `00` 是硬件填充——FlexCAN 总是发满 8 字节。

---

## 5. L-PDU 在接收路径上

接收方向，Can 驱动收到 L-PDU 后，CanIf 把它**反向转换**回 N-PDU（`PduInfoType`）：

```c
// CanIf.c — MCAL RX 回调
static void CanIf_McalRxCallback(Can_ControllerType Controller,
                                  uint8_t Hrh, const Can_PduType *PduInfo,
                                  const uint8_t *data)
{
    CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(PduInfo->id);

    if (pduId < CANIF_PDU_COUNT) {
        PduInfoType rxPdu = {
            .SduId      = pduId,              // PDU ID
            .SduLength  = PduInfo->length,    // DLC
            .SduDataPtr = (uint8_t *)data,    // ★ 指向 Can_PduType.data[]
        };
        CanIf_RxIndication(pduId, &rxPdu);    // 往上走
    }
}
```

注意 L-PDU 的 `data[8]` 数组直接被复用为 N-PDU 的数据指针——没有拷贝，只是换了个身份。

---

## 6. 总结

| 问题 | 答案 |
|------|------|
| L-PDU 是什么类型？ | `Can_PduType`（唯一不是 `PduInfoType` 的 PDU） |
| 为什么是定长 8 字节？ | CAN 2.0 协议硬性限制 |
| 谁产生 L-PDU？ | CanIf_Transmit（TX 方向） |
| 谁消费 L-PDU？ | Can_Write（MCAL 驱动） |
| 和 I-PDU/N-PDU 什么关系？ | 同一份数据，不同层的"外壳" |

---

## 参考文件

| 文件 | 内容 |
|------|------|
| `mcu/MCAL/Can/include/Can.h` | `Can_PduType` 定义 |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | N-PDU → L-PDU 转换实现 |
| `mcu/EcuAbstraction/CanIf/config/CanIf_Cfg.c` | PDU ID → CAN ID 映射表 |
| `mcu/include/ComStack_Types.h` | `PduInfoType`（I-PDU/N-PDU 共用） |
