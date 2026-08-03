# PduR — PDU Router 路由层详解

> AUTOSAR CAN 通信栈系列第 7 篇。建议先读 CanTp（第 6 篇），再读这篇，最后读 Com（第 9 篇）——PduR 正好在它们中间。
> 适合嵌入式小白和 AUTOSAR 小白。

---

## 先想一个问题：为什么要 PduR？

COM 层打包好了 I-PDU 要发出去，下层有 CanTp、有 CanIf，还可能走 SPI、LIN 等其他总线。**COM 不需要知道数据走哪条路**——它只管把 I-PDU 扔给一个"中转站"，让中转站决定路线。

这个中转站就是 PduR（PDU Router）。

```
                    COM (信号 → I-PDU)
                         │
                         ▼
                    ┌─────────┐
                    │  PduR   │  ← ★ 本文主角
                    │ (路由器) │
                    └────┬────┘
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
           CanTp       CanIf       SpiIf    ...
           (多帧)      (直传)      (SPI)
```

现实中的类比：快递分拣中心。包裹（I-PDU）送来后，根据目的地走不同的物流通道。

---

## 1. PduR 在栈中的位置

```
应用层:  SWC
         │  Rte
BSW:     ▼
         Com          ← 信号与 I-PDU
         │
         ▼
         PduR         ← ★ 路由器 (本文)
         │
    ┌────┴────┐
    ▼         ▼
  CanTp     CanIf       (当前直传路径未实现, 全走 CanTp)
    │         │
    └────┬────┘
         ▼
        Can           ← 硬件驱动
```

PduR 是 Com 之下、CanTp/CanIf 之上的**唯一出入口**。Com 发的所有数据都经过 PduR，CanTp 重组完的数据也都经过 PduR 回到 Com。

---

## 2. PduR 的核心功能——就一件事：转发

PduR 的全部代码只有 ~90 行——因为它不处理数据内容，只做**转发**：

| 方向 | 从哪来 | 到哪去 | 函数 |
|------|--------|--------|------|
| TX ↓ | Com | CanTp | `PduR_ComTransmit` |
| RX ↑ | CanIf | CanTp | `PduR_CanIfRxIndication` |
| RX ↑↑ | CanTp | Com | `PduR_CanTpRxIndication` |
| TX 确认 ↑ | CanIf | CanTp | `PduR_CanIfTxConfirmation` |
| TX 确认 ↑↑ | CanTp | Com | `PduR_CanTpTxConfirmation` |

用图表示：

```
TX 路径 (自上而下):
  Com ──PduR_ComTransmit─────→ CanTp ──→ CanIf ──→ Can ──→ 总线

RX 路径 (自下而上):
  总线 ──→ Can ──→ CanIf ──PduR_CanIfRxIndication──→ CanTp (重组)
                                                         │
                                          PduR_CanTpRxIndication (重组完)
                                                         │
                                                         ▼
                                                        Com

确认路径 (自下而上):
  Can ──→ CanIf ──PduR_CanIfTxConfirmation──→ CanTp (N-PDU 确认)
                                                  │
                                   PduR_CanTpTxConfirmation (I-PDU 确认)
                                                  │
                                                  ▼
                                                 Com
```

---

## 3. 核心 API 详解

### 3.1 TX 路径：PduR_ComTransmit

```c
Std_ReturnType PduR_ComTransmit(PduIdType PduId, const PduInfoType *PduInfoPtr)
{
    return CanTp_Transmit(PduId, PduInfoPtr);
}
```

Com 调用此函数发送 I-PDU。当前所有数据统一走 CanTp：
- ≤7 字节 → CanTp 以 **SF**（单帧）发送
- >7 字节 → CanTp 以 **FF+CF**（多帧）分段发送

### 3.2 RX 路径：两步走

**第一步** ——硬件收到 CAN 帧，CanIf 通知上来：

```c
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    CanTp_RxIndication(RxPduId, PduInfoPtr);  // 交给 CanTp 处理 (含 PCI)
}
```

此时数据还是**带 PCI 头的 N-PDU**（比如 `06 03 00 00 00 AA 55`）。CanTp 负责：判断帧类型（SF/FF/CF）、多帧重组、去掉 PCI。

**第二步** ——CanTp 处理完了，I-PDU 数据准备好了：

```c
void PduR_CanTpRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    Com_RxIndication(RxPduId, PduInfoPtr);  // 交给 Com 解包信号
}
```

此时数据是**纯 I-PDU**（不含 PCI），Com 直接拿去解包信号。

### 3.3 确认路径：同样两步走

CAN 驱动发完一帧后，通知会逐层往上传：

```
Can 发完一帧
  → CanIf_TxConfirmation
    → PduR_CanIfTxConfirmation → CanTp_TxConfirmation (N-PDU 级确认)

CanTp 发出最后一段 CF 后，整包 I-PDU 发完:
  → PduR_CanTpTxConfirmation → Com_TxConfirmation (I-PDU 级确认)
```

为什么是两步？因为多帧传输（FF+CF）时，一帧 ≠ 一包。CanTp 负责把多个 N-PDU 确认**聚合成一个** I-PDU 确认，再通知 Com。

---

## 4. PduR 的路由表（已配置但未使用）

PduR 配了一张路由表 `PduR_RouteConfig[]`，每条记录描述了源→目标的映射：

```c
typedef struct {
    uint16_t src_pdu_id;    // 源 PDU ID
    uint16_t dst_pdu_id;    // 目标 PDU ID
    uint8_t  src_module;    // 源模块: 0=Com, 1=CanIf, 2=SpiIf
    uint8_t  dst_module;    // 目标模块
} PduR_RouteConfigType;
```

**当前状态**：路由表已定义，但代码中**硬编码了所有路由**（全部走 CanTp）。真正启用路由表后，PduR 可以根据配置决定：

- Com 发来的 I-PDU → 查表 → 走 CanTp（多帧）还是直接走 CanIf（直传）
- CanIf 收到的帧 → 查表 → 给 CanTp（需要重组）还是直接给 Com（单帧直收）

---

## 5. 数据在各层叫什么——命名终于统一了

这是 AUTOSAR 学习中最容易晕的概念。同一个物理载荷，在不同层有不同的**角色名**：

| 层 | 叫法 | 特点 | 例子 |
|----|------|------|------|
| Com | **I-PDU** | 纯数据，无协议头 | `03 00 00 00 AA 55` |
| CanTp | **N-PDU** | 加了 PCI 分包头 | `06 03 00 00 00 AA 55`（SF） |
| Can | **L-PDU** | 硬件 CAN 帧，定长 8 字节 | CAN 帧 data[8] |

PduR 不改变数据内容——它只是在不同层之间**转发数据指针**。CanTp 在中间做 PCI 的加减。

```
Com 层看数据: "这是 I-PDU，我要解包信号"
              │
PduR 层看数据: "这是 PduInfoType (SduId + SduLength + SduDataPtr)，转发给 CanTp"
              │
CanTp 层看数据: "这是 N-PDU，我要看 PCI 字节决定怎么处理"
              │
Can 层看数据: "这是 L-PDU，8 字节物理帧，扔给硬件"
```

PduR 是唯一一个**不改变数据、不改变叫法、只是传递指针**的模块。

---

## 6. 为什么 PduR 只有 90 行？

因为它做的事太简单了——**调用链转发**。没有复杂算法，没有状态机：

```c
// 所有函数的模式都一样:
void PduR_XXX(PduIdType id, const PduInfoType *data) {
    LOG_D("PduR", "XXX: PduId=%u", id);   // 日志
    LowerLayer_YYY(id, data);              // 转发
}
```

真正的工作都在 Com（编解码 + 调度）和 CanTp（PCI 编解码 + 流控状态机）里做。PduR 就是一个**接线板**。

但这不代表它不重要——没有它，Com 和 CanTp 就得直接耦合，加一条新总线（比如 SPI、LIN）就得改 Com 的代码。有了 PduR，加新总线只需要在路由表里加一条记录。

---

## 7. 当前实现 vs 完整 AUTOSAR PduR

| 功能 | 状态 | 说明 |
|------|------|------|
| Com → CanTp 发送 | ✅ | 所有 I-PDU 统一走 CanTp |
| CanIf → CanTp 接收 | ✅ | 含 PCI 的 N-PDU 交给 CanTp 处理 |
| CanTp → Com 接收 | ✅ | 重组完成的 I-PDU 交给 Com 解包 |
| CanIf → CanTp TX 确认 | ✅ | N-PDU 级发送确认 |
| CanTp → Com TX 确认 | ✅ | I-PDU 级发送确认 |
| **Com → CanIf 直传** | ❌ | COM ≤8 字节直接发，不经过 CanTp |
| **CanIf → Com 直收** | ❌ | 收到的帧不经过 CanTp，直接给 Com |
| **路由表驱动** | ❌ | 动态查表决定路径，当前硬编码 |
| **多总线路由** | ❌ | Com ↔ SpiIf（SPI）、Com ↔ LinIf（LIN） |

---

## 8. 和 Com、CanTp 的配合——一次完整的收发

以我们实测通过的例子来讲。

### TX（发 6 字节 I-PDU）

```
Com: "我打包好了 I-PDU，6 字节，你帮我发"
  │
  │ PduR_ComTransmit(PduId=0, data=6 bytes)
  ▼
PduR: "好，走 CanTp"
  │
  │ CanTp_Transmit(PduId=0, data=6 bytes)
  ▼
CanTp: "6 ≤ 7，走 SF 单帧"
  │ 加 PCI 字节 0x06 → N-PDU = 06 03 00 00 00 AA 55
  │
  │ CanIf_Transmit(PduId=0, N-PDU)
  ▼
CanIf → Can → 总线
       candump 看到: can0 123 [8] 06 03 00 00 00 AA 55 00
```

### RX（收到带 PCI 的帧）

```
总线 → candump 发出: can0 100#07AABBCCDDEEFF00 (SF, 7字节数据)

Can 硬件收到 → CanIf:
  │
  │ CanIf_RxIndication(PduId=1, N-PDU=8 bytes 含PCI)
  ▼
PduR: "CanIf 来说收到帧了，给 CanTp 处理"
  │
  │ CanTp_RxIndication(PduId=1, N-PDU)
  ▼
CanTp: "PCI=0x07, SF, 7 字节数据"
  │ 去掉 PCI → I-PDU = AA BB CC DD EE FF 00 (7 bytes)
  │
  │ PduR_CanTpRxIndication(PduId=1, I-PDU=7 bytes)
  ▼
PduR: "CanTp 重组完了，I-PDU 给 Com"
  │
  │ Com_RxIndication(PduId=1, I-PDU=7 bytes)
  ▼
Com: "解包信号 TestRxData = 0x00FFEEDDCCBBAA"
  │ Update Bit = 1, Status = OK
  ▼
SWC 轮询到 Update Bit → 读信号 → 蓝灯亮！
```

---

## 参考文件

| 文件 | 内容 |
|------|------|
| `mcu/Services/PduR/include/PduR.h` | PduR API 声明 |
| `mcu/Services/PduR/src/PduR.c` | PduR 实现（~90 行） |
| `mcu/Services/PduR/config/PduR_Cfg.h` | 路由表配置结构 |
| `mcu/include/ComStack_Types.h` | `PduInfoType` — 全栈统一的 PDU 数据结构 |
