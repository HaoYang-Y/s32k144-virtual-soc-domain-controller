# CanIf — CAN Interface 层详解

> 从零开始，逐函数讲解 CanIf 层——如何把"裸 CAN ID + Mailbox 索引"抽象为"PDU ID"，让上层代码与硬件彻底解耦。

---

## 阅读前你需要知道

本文假设你已经读过：

| 前置知识 | 文档 |
|----------|------|
| MCAL Can 驱动（Can_Write/Can_Read 怎么用） | [./2_MCAL_Can驱动详解.md](./2_MCAL_Can驱动详解.md) |
| AUTOSAR 五层全景（CanIf 在整个架构中的位置） | [./1_AUTOSAR_CP_CAN通信栈.md](./1_AUTOSAR_CP_CAN通信栈.md) |

---

## 1. 为什么需要 CanIf？—— 从一段代码说起

MCAL Can 的 API 长这样：

```c
Can_Write(0, 0, &pdu);    // 发一帧：控制器编号=0, TX Mailbox=0, CAN ID=0x123
Can_Read(0, 1, &pdu);     // 收一帧：RX Mailbox=1 (过滤 CAN ID=0x100)
```

这段代码的问题是：**上层必须知道硬件细节**。换一个 MCU、改一下 Mailbox 布局、甚至只是把 TX MB 从索引 0 挪到索引 2，所有调用方都要改。这违反了一个基本设计原则——

> 上层代码不应该知道 CAN ID、Mailbox 编号、控制器编号。它只需要说"我要发 PDU 0"。

CanIf 要达成的效果：

```c
// 上层只和 PDU ID 打交道
CanIf_Transmit(0, &txIfPdu);          // "发 PDU 0" — 不需要知道 CAN ID 和 Mailbox
CanIf_RxIndication(0, &rxIfPdu);      // "收到了 PDU 1" — 不需要知道从哪个 MB 来的
```

**PDU ID 是 CanIf 层的核心抽象。** 一个 PDU = 一帧 CAN 报文。CanIf 内部用配置表把 PDU ID 映射到具体的 CAN ID 和控制器：

```
上层代码                      CanIf 层                      MCAL 层
────────                    ──────────                    ────────
"发 PDU 0"  ──→  查表: PDU 0 → CAN ID 0x123  ──→  Can_Write(0, MB0, {0x123, ...})
"收到 PDU 1" ←──  查表: CAN ID 0x100 → PDU 1  ←──  Can_Read(0, MB1, {0x100, ...})
```

---

## 2. 核心概念：PDU

PDU（Protocol Data Unit，协议数据单元）这个词贯穿整个 AUTOSAR 栈。在 CanIf 层，一个 PDU 就是**一帧 CAN 报文**。

### 2.1 CanIf_PduType — PDU 数据

```c
// CanIf.h 第 26~30 行
typedef struct {
    CanIf_PduIdType  id;      // PDU ID (0, 1, 2, ...)
    uint8_t          length;  // 数据长度 (0~8)
    uint8_t          *data;   // 指向数据缓冲区的指针
} CanIf_PduType;
```

与 MCAL `Can_PduType` 的对比：

| | Can_PduType (MCAL) | CanIf_PduType (CanIf) |
|--|-------------------|----------------------|
| ID 字段 | `uint32_t id` = CAN 报文 ID (如 0x123) | `uint16_t id` = PDU 逻辑编号 (如 0) |
| 数据 | `uint8_t data[8]` — 定长 8 字节数组 | `uint8_t *data` — 指针，灵活长度 |
| 帧属性 | 含 `is_extended` / `is_remote` | 无 — 上层不关心帧格式 |
| 依赖 | 与硬件 Mailbox 绑定 | 与硬件完全无关 |

### 2.2 CanIf_PduConfigType — PDU 到 CAN 的映射

```c
// CanIf_Cfg.h 第 18~23 行
typedef struct {
    uint16_t  pdu_id;         // PDU 逻辑编号（主键）
    uint8_t   controller_id;  // 使用哪个 CAN 控制器 (0 = FlexCAN0)
    uint32_t  can_id;         // 对应的 CAN 报文 ID
    uint8_t   dlc;            // DLC (Data Length Code)，数据长度码 (1~8)
} CanIf_PduConfigType;
```

**本项目当前的配置表**（由 `signals.yaml` 自动生成，位于 `CanIf_Cfg.c`）：

```c
const CanIf_PduConfigType CanIf_PduConfig[2] = {
    // {pdu_id, controller, can_id,     dlc}
    {0U,       0U,         0x00000123UL, 8U},   // PDU 0 → TX CAN ID 0x123
    {1U,       0U,         0x00000100UL, 8U},   // PDU 1 → RX CAN ID 0x100
};
```

配套的 PDU ID 宏（`CanIf_PduId.h`）：

```c
#define CANIF_PDU_ID_TX_0x123   0U   // "向 0x123 发送"的那个 PDU
#define CANIF_PDU_ID_RX_0x100   1U   // "从 0x100 接收"的那个 PDU
#define CANIF_PDU_COUNT         2U
```

---

## 3. 配置从哪来？—— YAML → 代码生成

CanIf 的配置不是手写的，而是从 `mcu/config/signals.yaml` 自动生成的。这是项目"单一数据源"原则的体现：

```
signals.yaml (手写，单一数据源)
      │
      ▼
generate_canif_cfg.py (Python 脚本)
      │
      ├──→ CanIf_Cfg.c      — CanIf_PduConfig[] 数组
      └──→ CanIf_PduId.h    — PDU ID 宏定义
```

### 生成逻辑

1. 遍历 YAML 中所有 `signals`，按 `(can_id, direction)` 分组——同 CAN ID 的 tx/rx 各成一个 PDU
2. 给每个 PDU 分配递增的 `pdu_id`（从 0 开始）
3. 生成 `CANIF_PDU_ID_{DIR}_{CAN_ID}` 格式的宏名
4. DLC 固定为 8（标准 CAN 帧）

### 新增一条 CAN 报文的流程

```yaml
# 只需在 signals.yaml 加一个信号条目：
signals:
  - name: NewSignal
    can_id: 0x200
    direction: rx
    ...
```

然后运行：

```bash
python3 mcu/tools/generate_canif_cfg.py mcu/config/signals.yaml
```

CanIf_Cfg.c 和 CanIf_PduId.h 自动更新，一条 C 代码都不用改。

---

## 4. 数据传输机制 — CanIf 如何把数据交给驱动

这是本文最核心的一节。用一个具体例子走通 TX 和 RX 的完整数据流。

### 4.0 先看全景

```
上层 (main.c/PduR)             CanIf 层                    MCAL 层 (Can_Write)
═══════════════════          ═══════════                  ═══════════════════
                              ┌─────────────────┐
CanIf_PduType {               │ CanIf_PduConfig[]│        Can_PduType {
  .id     = PDU_ID (0)  ──→  │  查表: PDU 0     │ ──→    .id     = 0x123
  .length = 8                 │  → can_id=0x123  │        .length = 8
  .data   → txBuf[8]    ──→  │  → 逐字节拷贝    │ ──→    .data[8]= {...}
}                             └─────────────────┘        }
                                                               │
                                                     Can_Write(0, MB0, &pdu)
                                                               │
                                                       FlexCAN 硬件 → CAN 总线
```

**CanIf 的职责就一件事**：把上层的 PDU ID（逻辑编号）翻译成 CAN ID（硬件地址），然后原样转发数据。

### 4.1 TX 路径详解：从 PDU 到 CAN 帧

假设上层要发一帧数据到 CAN ID `0x123`，payload = `{0x01, 0x02, 0xAA, 0x55, 0xAA, 0x55, 0x00, 0x00}`。

**第 1 步：上层构建 PDU**

```c
// main.c — 上层只填 PDU ID + 数据，不填 CAN ID
uint8_t txData[8] = {0x01, 0x02, 0xAA, 0x55, 0xAA, 0x55, 0x00, 0x00};

CanIf_PduType txPdu = {
    .id     = CANIF_PDU_ID_TX_0x123,   // 只说"我要发 PDU 0"
    .length = 8U,
    .data   = txData                    // 指向数据缓冲区
};

CanIf_Transmit(0, &txPdu);
```

此时 `txPdu` 里**没有 CAN ID**——只有 PDU 编号 0。

**第 2 步：CanIf 查配置表，翻译 PDU ID → CAN ID**

```c
// CanIf.c — CanIf_Transmit() 内部
// 配置表里存的映射关系 (从 signals.yaml 自动生成):
//
//   CanIf_PduConfig[0] = {.pdu_id=0, .can_id=0x123, .controller=0, .dlc=8}
//   CanIf_PduConfig[1] = {.pdu_id=1, .can_id=0x100, .controller=0, .dlc=8}

const CanIf_PduConfigType *cfg = CanIf_FindConfigByPduId(txPdu.id);  // PDU 0 → can_id=0x123
```

**第 3 步：格式转换 — CanIf_PduType → Can_PduType**

```c
// CanIf.c — 构造 MCAL 层认识的格式
Can_PduType canPdu = {0};            // 全量零初始化

canPdu.id     = cfg->can_id;         //  0x123  ← 从配置表来，不是上层传的！
canPdu.length = txPdu.length;        //  8

// 逐字节拷贝：指针 → 定长数组
canPdu.data[0] = txPdu.data[0];     // 0x01
canPdu.data[1] = txPdu.data[1];     // 0x02
canPdu.data[2] = txPdu.data[2];     // 0xAA
// ... 拷贝全部 8 字节
```

转换前后的对比：

| | 转换前 (CanIf_PduType) | 转换后 (Can_PduType) |
|--|----------------------|---------------------|
| ID | `0` (PDU 编号) | `0x123` (CAN 报文 ID) |
| 数据 | `uint8_t *data` 指向 `txData[8]` | `uint8_t data[8]` 定长数组 |
| 帧类型 | 无 | `is_extended=false, is_remote=false` |

**第 4 步：交给 MCAL 驱动**

```c
// CanIf.c — 最后一步：调用 MCAL
status_t ret = Can_Write(0, CANIF_TX_HTH(=0), &canPdu);
//                                                      ↑
//                          Can_PduType 已包含完整的 CAN ID + 数据
//                          Can_Write 内部: CAN_ConfigTxBuff → CAN_Send → 硬件发送
```

**完整数据流（一次 TX 的字节级追踪）**：

```
main.c                          CanIf.c                     MCAL Can.c             硬件
───────                        ────────                    ──────────            ──────
txData[8] = {0x01,0x02,        CanIf_Transmit()
             0xAA,0x55,          │
             0xAA,0x55,          ├─ 查表: PDU 0
             0x00,0x00}          │   → can_id=0x123
             │                   │
txPdu.id=0   │                   ├─ 构造 canPdu:
txPdu.len=8  │                   │   .id=0x123
txPdu.data ──┼──→ ─ ─ ─ ─ ─ ─ → │   .data[0]=0x01
             │    (指针传递,      │   .data[1]=0x02    Can_Write(0,MB0,&canPdu)
             │     不拷贝数据)    │   ...                │
             │                   │   .data[7]=0x00      ├─ CAN_ConfigTxBuff(MB0)
             │                   │                      ├─ CAN_Send(MB0)
             │                   └─ Can_Write(...) ───→ └─ FLEXCAN_DRV_Send()
                                                                               │
                                                                        CAN_H/CAN_L
                                                                        总线上的帧:
                                                                        ID=0x123
                                                                        DLC=8
                                                                        data=01 02 AA 55...
```

### 4.2 RX 路径详解：从 CAN 帧到 PDU

RX 方向是 TX 的逆过程。

**第 1 步：MCAL 收到一帧**

```c
// main.c 轮询 MCAL
Can_PduType rxCanPdu;
if (Can_Read(0, 1, &rxCanPdu) == STATUS_SUCCESS) {
    // rxCanPdu.id     = 0x100  ← 硬件收到的 CAN ID
    // rxCanPdu.length = 8
    // rxCanPdu.data   = {...}   ← 定长数组
```

**第 2 步：CAN ID → PDU ID 反查**

```c
    // main.c — 把 CAN ID 翻译成 PDU ID
    CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(rxCanPdu.id);
    // 0x100 → 查 CanIf_PduConfig[] → 找到 PDU 1
```

**第 3 步：构造 CanIf_PduType，通知 CanIf 层**

```c
    CanIf_PduType rxIfPdu = {
        .id     = pduId,            // 1 = CANIF_PDU_ID_RX_0x100
        .length = rxCanPdu.length,  // 8
        .data   = rxCanPdu.data     // 直接指向 Can_PduType 的 data[8] 数组
    };
    CanIf_RxIndication(0, &rxIfPdu);
```

**关键区别**：RX 方向 `CanIf_PduType.data` **直接指向** `Can_PduType.data[8]` 数组——不拷贝数据，零开销。TX 方向必须拷贝（因为 `uint8_t *data` 指向的缓冲区可能在上层函数返回后失效）。

### 4.3 TX vs RX 数据拷贝策略

| | TX (CanIf_Transmit) | RX (CanIf_RxIndication) |
|--|---------------------|------------------------|
| 数据方向 | 上层 → CanIf → MCAL | MCAL → CanIf → 上层 |
| 数据拷贝 | **需要拷贝**（逐字节） | **零拷贝**（直接传指针） |
| 原因 | 上层缓冲区可能在 `CanIf_Transmit` 返回后释放 | `Can_PduType.data[8]` 在 `Can_Read` 的调用栈上，生命周期覆盖整个处理过程 |
| ID 转换 | PDU ID → CAN ID（查 `pdu_id` 字段） | CAN ID → PDU ID（查 `can_id` 字段） |

---

## 5. 核心 API

### 5.1 CanIf_Init — 对标 AUTOSAR SWS_CanIf_00013

```c
void CanIf_Init(void);
```

> **AUTOSAR 规范定义**：初始化 CanIf 模块及所有已配置的 CAN 控制器。在 AUTOSAR 标准中，BSW 模块初始化由 EcuM（ECU Manager）统一调度：`MCAL Init → ECU Abstraction Init → Services Init → RTE Init`。本项目由 `EcuM_Init()` 按此顺序调用各模块的 `_Init()`。

当前实现简单设置内部状态标志 `canif_state = 1`（已初始化），后续所有 API 调用都会检查此标志。

**调用链条**：

```c
// main.c — MCAL 硬件初始化在此（依赖具体硬件配置结构体）
Can_Init(&can0_cfg);
Can_SetControllerMode(0, CAN_CS_STARTED);

// EcuM.c — BSW 模块按 AUTOSAR 顺序初始化
EcuM_Init()
  └── CanIf_Init();          // ← 由 EcuM 统一调用，不在 main.c 中直接调
  //    SpiIf_Init();        // TODO
  //    PduR_Init();         // TODO
  //    Com_Init();          // TODO
```

---

### 5.2 CanIf_Transmit — 对标 AUTOSAR SWS_CanIf_00050

```c
uint8_t CanIf_Transmit(CanIf_ControllerType Controller, CanIf_PduType *PduPtr);
// 返回: E_OK (0) = 成功, E_NOT_OK (1) = 失败
```

> **AUTOSAR 规范定义**：请求发送一个 PDU。CanIf 根据 PDU ID 查配置表找到对应的 CAN ID 和控制器，然后调用 `Can_Write` 完成硬件发送。函数立即返回，实际发送由硬件异步完成。

| 参数 | 含义 | 本项目实际值 |
|------|------|-------------|
| `Controller` | CAN 控制器编号 | 0 = FlexCAN0 |
| `PduPtr` | 指向待发送 PDU | `{id=CANIF_PDU_ID_TX_0x123, length=8, data→txBuf}` |

**内部流程**（每一步都对应 AUTOSAR SWS_CanIf 的要求）：

```
CanIf_Transmit(Controller=0, PduPtr)
  │
  ├── 1. DET 检查（开发错误检测，SWS_CanIf 要求）
  │      · canif_state == 1 ? → 否则报告 CANIF_E_UNINIT
  │      · PduPtr != NULL ?   → 否则报告 CANIF_E_PARAM
  │
  ├── 2. PDU 查表（SWS_CanIf_00050 核心步骤）
  │      cfg = CanIf_FindConfigByPduId(PduPtr->id)
  │      例如 PDU 0 → {can_id=0x123, controller=0, dlc=8}
  │      找不到 → 报告 CANIF_E_INVALID_PDU_ID → 返回 E_NOT_OK
  │
  ├── 3. 格式转换：CanIf_PduType → Can_PduType
  │      canPdu = {0};              ← ⚠️ 全量零初始化!（is_extended=false, is_remote=false）
  │      canPdu.id     = cfg->can_id       // 0x123（从配置表取，不是从 PduPtr 取）
  │      canPdu.length = PduPtr->length    // 8
  │      for i: canPdu.data[i] = PduPtr->data[i]  // 逐字节拷贝
  │
  └── 4. 调用 MCAL
           ret = Can_Write(Controller, CANIF_TX_HTH(=0), &canPdu)
           → STATUS_SUCCESS → 返回 E_OK
           → STATUS_ERROR   → LOG_E + 返回 E_NOT_OK
```

> ⚠️ **踩坑记录**：第 3 步 `canPdu = {0}` 必须全量初始化。我们最初只赋值了 `id`/`length`/`data`，漏了 `is_extended` 和 `is_remote`。栈上的垃圾值导致标准帧被当扩展帧发出，CANable 完全收不到。板子指示灯全正常，就是总线上没数据。根因是 `Can_PduType` 含 `bool` 字段（见 [2_MCAL_Can驱动详解 §3.1](./2_MCAL_Can驱动详解.md) 踩坑记录），CanIf 的修复方法是 `= {0}` 全量零初始化。

**调用示例**：

```c
uint8_t txData[8] = {0x01, 0x02, 0xAA, 0x55, 0xAA, 0x55, 0x00, 0x00};
CanIf_PduType txPdu = {
    .id     = CANIF_PDU_ID_TX_0x123,  // 只需要 PDU ID，不需要 CAN ID
    .length = 8U,
    .data   = txData
};
uint8_t ret = CanIf_Transmit(0, &txPdu);
if (ret != E_OK) {
    // TX 失败处理
}
```

---

### 5.3 CanIf_RxIndication — 对标 AUTOSAR SWS_CanIf_00030

```c
void CanIf_RxIndication(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr);
```

> **AUTOSAR 规范定义**：CAN 驱动收到一帧数据后，通过此函数向 CanIf 层通知。CanIf 校验 PDU ID 合法性后，将数据**转发给 PduR 层**（当前为 TODO，PduR 层实现后补全）。

**调用方**：当前由 `main.c` 的轮询循环调用（未来由中断服务调用）：

```c
// main.c 中的 RX 轮询
Can_PduType rxCanPdu;
if (Can_Read(0, RX_MB, &rxCanPdu) == STATUS_SUCCESS) {
    // CAN ID → PDU ID 转换（CanIf 层职责）
    CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(rxCanPdu.id);
    if (pduId < CANIF_PDU_COUNT) {
        CanIf_PduType rxIfPdu = {.id = pduId, .length = rxCanPdu.length, .data = rxCanPdu.data};
        CanIf_RxIndication(0, &rxIfPdu);   // ← 通知 CanIf 层
    }
}
```

**内部流程**：DET 检查 → PDU ID 校验（查表确认存在）→ 日志记录 → [预留] PduR 转发。

---

### 5.4 CanIf_TxConfirmation — 对标 AUTOSAR SWS_CanIf_00040

```c
void CanIf_TxConfirmation(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr);
```

> **AUTOSAR 规范定义**：CAN 驱动发送完成后，通过此函数向 CanIf 层确认。CanIf 处理完成后**转发确认给 PduR 层**。

当前实现：DET 检查 + 日志记录。PduR 转发预留。

---

### 5.5 CanIf_FindPduIdByCanId — CAN ID → PDU ID 反查

```c
CanIf_PduIdType CanIf_FindPduIdByCanId(uint32_t CanId);
```

RX 路径专用辅助函数。轮询收到 `Can_PduType`（含 CAN ID 0x100）后，查表找到对应的 PDU ID (=1)。线性扫描 `CanIf_PduConfig[]`，匹配 `can_id` 字段。未找到时返回 `CANIF_PDU_COUNT`。

**为什么需要这个函数？** 因为硬件给的是一帧带 CAN ID 的裸数据，但 CanIf 层的上层（PduR）需要的是 PDU ID。这个函数做的就是 CAN 层→CanIf 层的"翻译"。

---

## 6. DET 开发错误检测

CanIf 实现了 AUTOSAR DET（Development Error Tracer，开发错误追踪），在开发阶段通过日志暴露编程错误。DET 是 AUTOSAR 标准化的错误报告机制（SWS_CanIf 的 DET 章节要求每个 API 在入参非法时通过 DET 上报 ModuleId + ApiId + ErrorId）。

| 错误类型 | Error ID | 检查条件 | 触发场景 |
|----------|----------|---------|---------|
| `CANIF_E_UNINIT` | 0x02 | `canif_state == 0` | 还没调 `CanIf_Init()` 就调了收发函数 |
| `CANIF_E_PARAM` | 0x01 | `PduPtr == NULL` | 传了空指针 |
| `CANIF_E_INVALID_PDU_ID` | 0x03 | PDU ID 不在配置表中 | 用了未定义的 PDU ID |

每个错误日志都包含三段定位信息：

```
LOG_E("CanIf", "Transmit: not initialized (Mod=0x32 Api=0x01 Err=0x02)")
                 ────────────────  ────────  ────────  ────────
                         │             │         │         │
                     错误描述      模块=CanIf  调用者  原因=未初始化
```

**编译开关**（`CanIf_Cfg.h`）：开发阶段设为 `CANIF_DEV_ERROR_DETECT = STD_ON`，量产时改 `STD_OFF` 移除所有 DET 代码，零运行时开销。

---

## 7. 收发路径速查

> 详细字节级追踪见 [第 4 节：数据传输机制](#4-数据传输机制--canif-如何把数据交给驱动)。

**TX**: `main.c → CanIf_Transmit() → 查表(PDU→CAN ID) → Can_Write() → 硬件`

**RX**: `硬件 → Can_Read() → CanIf_FindPduIdByCanId(CAN ID→PDU) → CanIf_RxIndication()`

---

## 8. 改造前后对比

### 改造前 — 裸 Can API

```c
#include "Can.h"

// 必须知道 CAN ID、Mailbox 索引
Can_PduType tx = {.id = 0x123UL, .length = 8U, .data = {...}};
Can_Write(0, 0, &tx);   // 控制器=0, MB=0

Can_PduType rx;
Can_Read(0, 1, &rx);    // 控制器=0, MB=1
// rx.id → 0x100, rx.data → ...
```

**问题**：上层代码知道 CAN ID（0x123/0x100）、知道 Mailbox 布局（TX=0, RX=1）。换硬件就得改代码。

### 改造后 — CanIf API

```c
#include "CanIf.h"

// 只需要 PDU ID
CanIf_PduType txPdu = {.id = CANIF_PDU_ID_TX_0x123, .length = 8U, .data = txBuf};
CanIf_Transmit(0, &txPdu);

Can_PduType rxCanPdu;
if (Can_Read(0, 1, &rxCanPdu) == STATUS_SUCCESS) {
    CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(rxCanPdu.id);
    CanIf_PduType rxIfPdu = {.id = pduId, .length = rxCanPdu.length, .data = rxCanPdu.data};
    CanIf_RxIndication(0, &rxIfPdu);
}
```

**改进**：

| | 改造前 | 改造后 |
|--|--------|--------|
| CAN ID 位置 | 硬编码在 main.c 中 | 集中在 `signals.yaml` + `CanIf_Cfg.c`，改 YAML 即可 |
| 新增报文 | 改 main.c，可能改 Mailbox 布局 | 加 YAML 条目 + 运行生成脚本，main.c 不用改 |
| 错误检测 | 无，参数错误可能导致 HardFault | DET 框架捕获 NULL 指针、未初始化、无效 PDU ID |
| 平台移植 | main.c 重写 | 只改 `CanIf_Cfg.c`（生成脚本适配新平台） |

---

## 9. 文件清单

| 文件 | 来源 | 说明 |
|------|------|------|
| `mcu/EcuAbstraction/CanIf/include/CanIf.h` | 手写 | 类型定义 + 5 个 API 声明 |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | 手写 | 4 个核心 API + 2 个查表辅助 + DET 框架 |
| `mcu/EcuAbstraction/CanIf/config/CanIf_Cfg.h` | 手写 | `CanIf_PduConfigType` 结构体定义 |
| `mcu/EcuAbstraction/CanIf/config/CanIf_Cfg.c` | **生成** | `CanIf_PduConfig[]` 数组 — 不要手改 |
| `mcu/EcuAbstraction/CanIf/config/CanIf_PduId.h` | **生成** | PDU ID 宏定义 — 不要手改 |
| `mcu/config/signals.yaml` | 手写 | 信号矩阵定义 — 单一数据源，改这里 |
| `mcu/tools/generate_canif_cfg.py` | 手写 | YAML → CanIf 配置生成脚本 |

---

## 10. 与上下层的关系

```
MCAL Can (2_MCAL_Can驱动详解.md) ──→ CanIf (本文) ──→ PduR ──→ Com ──→ RTE ──→ SWC
       ↑                              ↑
    已实现                         已实现                    待实现
```

- **向下**：`CanIf_Transmit()` → `Can_Write()`（MCAL）
- **向上**：`CanIf_RxIndication()` → `PduR_CanIfRxIndication()`（预留，PduR 实现后补全）
- **配置源**：`signals.yaml` → `generate_canif_cfg.py` → `CanIf_Cfg.c` + `CanIf_PduId.h`
