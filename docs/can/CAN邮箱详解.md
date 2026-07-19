# CAN 邮箱（Mailbox）详解

> 以本项目 S32K144 + FlexCAN 硬件为基础，从概念到寄存器，逐层讲解 CAN 邮箱的工作原理、
> 配置方法、状态机以及常见问题。

---

## 1. 什么是 CAN 邮箱？

### 1.1 一句话定义

**CAN 邮箱是 FlexCAN 控制器内部的一块硬件 RAM 区域，每个邮箱可以存储一条 CAN 报文，硬件自动完成帧匹配和收发搬运，无需 CPU 逐 bit 操作总线。**

### 1.2 直观类比

把 FlexCAN 控制器想象成一个**邮局**：

| 概念 | 类比 | 说明 |
|------|------|------|
| CAN 总线 | 公路 | 所有报文在上面传输 |
| CAN 邮箱（Mailbox） | 带编号的信箱 | 存储在控制器内部 RAM 中 |
| 邮箱编号（MB0, MB1...） | 信箱门牌号 | 硬件索引，从 0 开始 |
| CAN 帧（报文） | 一封信 | 包含 ID + 数据 |
| 配置为 RX 的邮箱 | 收信箱 | 硬件自动把匹配的信投进来 |
| 配置为 TX 的邮箱 | 发信箱 | CPU 把信放进去，硬件自动发出 |
| CODE 字段 | 信箱状态灯 | 告诉 CPU 这个邮箱是空的/满的/正在用 |

### 1.3 为什么要用邮箱？

如果没有邮箱机制，CPU 需要用 GPIO 模拟 CAN 时序，逐 bit 采样和发送——这几乎不可行。有了邮箱后：

- **硬件接管**：帧匹配、CRC 校验、位时序、重发机制全部由 FlexCAN 硬件完成
- **异步解耦**：CPU 把报文写入 TX 邮箱后就可以去做别的事，硬件在总线空闲时自动发送
- **自动过滤**：RX 邮箱配置好 ID 过滤器后，硬件只在收到匹配报文时才通知 CPU，避免每个帧都触发中断

---

## 2. 硬件邮箱的结构

### 2.1 每个邮箱的存储器布局

每个邮箱在 FlexCAN 嵌入式 RAM 中占据固定大小的空间。在 SDK 中以结构体 `flexcan_msgbuff_t` 表示（来自 [flexcan_driver.h](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/drivers/inc/flexcan_driver.h)）：

```c
typedef struct {
    uint32_t cs;          // Code and Status  —— 控制/状态字
    uint32_t msgId;       // Message Buffer ID —— CAN ID
    uint8_t  data[64];    // 数据区（最大 64 字节，支持 CAN FD）
    uint8_t  dataLen;     // 有效数据长度
} flexcan_msgbuff_t;
```

| 字段 | 位宽 | 作用 |
|------|------|------|
| `cs` | 32 bit | 包含 CODE（4 bit）、DLC、RTR、IDE、SRR、时戳等多个子字段 |
| `msgId` | 32 bit | 存储 CAN ID（标准 11-bit 或扩展 29-bit） |
| `data[]` | 最多 64 B | 报文有效载荷（标准 CAN 只用前 8 字节，CAN FD 可用全部 64 字节） |
| `dataLen` | 8 bit | 有效数据长度（0~8 或 0~64 for CAN FD） |

### 2.2 CODE 字段 — 邮箱的状态控制器

`cs` 寄存器的 bit 24~27 是 **CODE** 字段，这是邮箱最关键的 4 个 bit，控制邮箱的行为和状态。定义在 [flexcan_hw_access.h](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/drivers/src/flexcan/flexcan_hw_access.h)：

**RX 邮箱的 CODE 值：**

| CODE | 宏定义 | 含义 |
|------|--------|------|
| `0x0` | `FLEXCAN_RX_INACTIVE` | 邮箱未激活，不参与收发 |
| `0x1` | `FLEXCAN_RX_BUSY` | 硬件正在写入新帧，CPU 切勿访问 |
| `0x2` | `FLEXCAN_RX_FULL` | **新帧已收到**，等待 CPU 读取 |
| `0x4` | `FLEXCAN_RX_EMPTY` | 邮箱已激活、为空，**等待接收** |
| `0x6` | `FLEXCAN_RX_OVERRUN` | 溢出：新帧覆盖了未被读取的旧帧 |
| `0xA` | `FLEXCAN_RX_RANSWER` | 远程帧应答已收到 |
| `0xF` | `FLEXCAN_RX_NOT_USED` | 未使用（禁用） |

**TX 邮箱的 CODE 值：**

| CODE | 宏定义 | 含义 |
|------|--------|------|
| `0x8` | `FLEXCAN_TX_INACTIVE` | 邮箱未激活 ⚠️ **发送完成后自动变成此状态** |
| `0x9` | `FLEXCAN_TX_ABORT` | 发送被中止 |
| `0xC` | `FLEXCAN_TX_DATA` | 数据帧等待发送（或正在发送） |
| `0xE` | `FLEXCAN_TX_TANSWER` | 远程帧应答 |
| `0xF` | `FLEXCAN_TX_NOT_USED` | 未使用（禁用） |

### 2.3 cs 字段的完整位布局

```
cs (32 bit):
┌─────────────────┬──────┬─────┬─────┬─────┬─────┬──────────────┐
│   TIME_STAMP    │ CODE │ SRR │ IDE │ RTR │ DLC │   (reserved) │
│    bit 0~15     │24~27 │ 22  │ 21  │ 20  │16~19│              │
└─────────────────┴──────┴─────┴─────┴─────┴─────┴──────────────┘
```

- **TIME_STAMP** (bit 0~15)：来自 FlexCAN 内部自由运行定时器，帧到达/发送时的快照
- **DLC** (bit 16~19)：Data Length Code，有效数据长度（0~8）
- **RTR** (bit 20)：Remote Transmission Request，远程帧标志
- **IDE** (bit 21)：ID Extended，1 = 29-bit 扩展 ID，0 = 11-bit 标准 ID
- **SRR** (bit 22)：Substitute Remote Request（仅扩展帧）

---

## 3. 邮箱的生命周期

### 3.1 TX 发送邮箱：一个完整的发送周期

```
  CPU 写入报文并设置 CODE=0xC (TX_DATA)
                  │
                  ▼
  ┌──────────────────────────────┐
  │  状态: 等待总线空闲            │
  │  CODE = 0xC (TX_DATA)        │
  └──────────────┬───────────────┘
                 │ 总线空闲，硬件自动发送
                 ▼
  ┌──────────────────────────────┐
  │  状态: 发送完成               │
  │  CODE = 0x8 (TX_INACTIVE)    │  ← ⚠️ 不能再发送！必须重新配置！
  └──────────────────────────────┘
                 │
                 │ CPU 调用 CAN_ConfigTxBuff() 重新激活
                 │ CODE: 0x8 → 0xC
                 ▼
  ┌──────────────────────────────┐
  │  状态: 就绪，可再次发送        │
  │  CODE = 0xC (TX_DATA)        │
  └──────────────────────────────┘
```

**关键规则**：TX 邮箱发送完一帧后，CODE 自动回退到 `0x8`（`TX_INACTIVE`）。如果下次 `Can_Write()` 不重新配置就调用 `CAN_Send()`，PAL 驱动发现 CODE ≠ READY，会返回 `STATUS_BUSY`。这就是本项目 `Can.c` 中每次 `Can_Write()` 都先调 `CAN_ConfigTxBuff()` 的原因。

### 3.2 RX 接收邮箱：一个完整的接收周期

```
  初始化: CPU 设置 CODE=0x4 (RX_EMPTY) + ID 过滤器
                  │
                  ▼
  ┌──────────────────────────────┐
  │  状态: 空闲，等待匹配          │
  │  CODE = 0x4 (RX_EMPTY)       │
  └──────────────┬───────────────┘
                 │ 总线上出现匹配 ID 的帧
                 ▼
  ┌──────────────────────────────┐
  │  状态: 硬件正在写入            │
  │  CODE = 0x1 (RX_BUSY)        │  ← CPU 不能访问！数据可能不完整
  └──────────────┬───────────────┘
                 │ 硬件写入完成
                 ▼
  ┌──────────────────────────────┐
  │  状态: 新帧就绪               │
  │  CODE = 0x2 (RX_FULL)        │  ← 触发中断（RXF 标志）
  └──────────────┬───────────────┘
                 │ CPU 读取数据 (Can_Read / CAN_Receive)
                 │ 然后写 CODE=0x4 重新锁存
                 ▼
  ┌──────────────────────────────┐
  │  状态: 回到空闲               │
  │  CODE = 0x4 (RX_EMPTY)       │
  └──────────────────────────────┘

  ⚠️ 特殊情况：
  如果 CPU 没及时读取 RX_FULL，又来了一帧匹配的报文：
  CODE = 0x6 (RX_OVERRUN) —— 旧帧被覆盖，数据丢失！
```

---

## 4. S32K144 的邮箱资源

### 4.1 硬件邮箱总数

来自 [S32K144_features.h](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/include/S32K144_features.h)：

| FlexCAN 实例 | 最大邮箱数 | 用途示例 |
|-------------|-----------|----------|
| CAN0 | **32** 个 | 主力 CAN 通道 |
| CAN1 | **16** 个 | 辅助 CAN 通道 |
| CAN2 | **16** 个 | 辅助 CAN 通道 |

### 4.2 本项目的邮箱分配

来自 [Can_Cfg.h](../../mcu/MCAL/Can/config/Can_Cfg.h)：

```c
#define CAN_MAILBOX_COUNT         16U   // 每个控制器使用 16 个邮箱
#define CAN_RX_HARDWARE_OBJECTS    8U   // 其中 8 个配置为接收
#define CAN_TX_HARDWARE_OBJECTS    8U   // 其中 8 个配置为发送
```

邮箱索引分配规则：

```
MB0  ─── TX 邮箱 (Hth = 0)
MB1  ─── TX 邮箱 (Hth = 1)
 ...
MB7  ─── TX 邮箱 (Hth = 7)
MB8  ─── RX 邮箱 (Hrh = 0)
MB9  ─── RX 邮箱 (Hrh = 1)
 ...
MB15 ─── RX 邮箱 (Hrh = 7)
```

- **Hth** (Hardware Transmit Handle)：TX 邮箱句柄，范围 `[0, num_tx-1]`
- **Hrh** (Hardware Receive Handle)：RX 邮箱句柄，范围 `[0, num_rx-1]`
- 硬件邮箱索引 = `Hth`（TX）或 `num_tx + Hrh`（RX）

### 4.3 邮箱过滤器机制

RX 邮箱通过**硬件 ID 过滤**来匹配报文。FlexCAN 支持两种过滤：

| 过滤方式 | 机制 | 精确度 |
|----------|------|--------|
| 单 ID 匹配 | 邮箱 `msgId` 字段存储目标 ID，收到帧的 ID 必须完全相等 | 精确匹配 |
| 掩码匹配 | 使用全局 `RXMGMASK` 寄存器，可屏蔽不关心的 ID 位 | 范围匹配 |

示例：若 `RXMGMASK = 0x7F0`，邮箱 ID = `0x100`，则 ID 为 `0x100` ~ `0x10F` 的帧都会被此邮箱接收。

---

## 5. 邮箱在 AUTOSAR 各层中的抽象

```
  SWC 层                     看到的是: "发送信号 X" / "接收信号 Y"
  Com 层                     看到的是: I-PDU (信号组)
  PduR 层                    看到的是: PDU (数据包 + ID)
  CanIf 层                   看到的是: PDU → 邮箱句柄 Hth/Hrh 映射
  Can (MCAL) / PAL 层        看到的是: flexcan_msgbuff_t、CODE、寄存器
  ─────────────────────────────────────────────────────────────────
  FlexCAN 硬件               真实的 RAM 区域: MB0, MB1, MB2...
```

各层的关键类型对应：

| 层次 | 类型 | 代表什么 |
|------|------|----------|
| MCAL | `Can_HardwareObject` | 一个邮箱的 ID + 帧格式配置 |
| MCAL | `Can_PduType` | 一条完整的 CAN 报文（ID + DLC + data[]） |
| PAL | `can_message_t` | 同 MCAL 的 PDU，封装了 `cs` + `id` + `data[]` |
| DRV | `flexcan_msgbuff_t` | 硬件邮箱的完整镜像（cs + msgId + data[]） |
| DRV | `flexcan_mb_handle_t` | 邮箱句柄 = mb_message 指针 + 信号量 + 状态 |

---

## 6. 代码实战：如何配置和使用邮箱

### 6.1 APP 层配置示例

来自 [main.c](../../mcu/App/Swc_SignalGateway/src/main.c) 的实际代码：

```c
// 邮箱分配
#define TX_MB   0   // 使用第 0 个 TX 邮箱
#define RX_MB   1   // 使用第 1 个 RX 邮箱（硬件索引 = num_tx + 0）

// TX 邮箱配置 —— 发送 CAN ID 0x123
can_buff_config_t can0_tx_cfg = {
    .enableFD  = false,
    .enableBRS = false,
    .idType    = CAN_MSG_ID_STD,  // 标准帧 (11-bit ID)
    .isRemote  = false,
};

// RX 邮箱配置 —— 接收 CAN ID 0x100
can_buff_config_t can0_rx_cfg = {
    .enableFD  = false,
    .enableBRS = false,
    .idType    = CAN_MSG_ID_STD,
    .isRemote  = false,
};

// 全局 CAN 配置
can_user_config_t can0_cfg = {
    .max_num_mb        = 16,   // 共 16 个邮箱
    .num_tx_mailboxes  = 1,    // 只用 1 个 TX 邮箱
    .num_rx_mailboxes  = 1,    // 只用 1 个 RX 邮箱
    .tx_mailboxes      = &tx_mb_cfg,  // TX 邮箱的 ID 数组
    .rx_mailboxes      = &rx_mb_cfg,  // RX 邮箱的 ID 数组
};
```

### 6.2 MCAL 层初始化流程

来自 [Can.c](../../mcu/MCAL/Can/src/Can.c) 的 `Can_Init()`：

```c
// 步骤 1: 初始化所有 TX 邮箱
for (i = 0; i < config->num_tx_mailboxes; i++) {
    CAN_ConfigTxBuff(&Can_Instance, i, &Can_TxBuffCfg);
}

// 步骤 2: 初始化所有 RX 邮箱（带 ID 过滤）
for (i = 0; i < config->num_rx_mailboxes; i++) {
    CAN_ConfigRxBuff(&Can_Instance, config->num_tx_mailboxes + i,
                     &Can_RxBuffCfg, config->rx_mailboxes[i].id);
}
```

### 6.3 发送一条报文

```c
// 1. 准备报文
Can_PduType tx_msg = {
    .id          = 0x123,
    .length      = 8,
    .is_extended = false,
    .data        = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88},
};

// 2. 重新配置 TX 邮箱（必须！因为上次发送后 CODE 变成了 INACTIVE）
CAN_ConfigTxBuff(&Can_Instance, TX_MB, &Can_TxBuffCfg);

// 3. 发送
Can_Write(TX_MB, &tx_msg);
```

### 6.4 接收一条报文

```c
Can_PduType rx_msg;

// 阻塞读取 RX 邮箱
Can_Read(RX_MB, &rx_msg);

// 此时 rx_msg.id、rx_msg.length、rx_msg.data[] 已被填充
```

---

## 7. 常见问题与排查

### 7.1 STATUS_BUSY — TX 邮箱发送失败

**现象**：调用 `Can_Write()` 返回 `STATUS_BUSY`（值 2），报文发不出去。

**底层链路**：
```
Can_Write() → CAN_Send() → FLEXCAN_DRV_Send() → FLEXCAN_StartSendData()
                                                    │
                                    检查 state->mbs[mb_idx].state != FLEXCAN_MB_IDLE
                                                    │
                                          返回 STATUS_BUSY
```

**原因**：上次发送后，硬件把 TX 邮箱的 CODE 设成了 `TX_INACTIVE`（0x8），PAL 驱动内部 `state->mbs[mb_idx].state` 不是 `FLEXCAN_MB_IDLE`，返回 `STATUS_BUSY`。

**解决**：**每次 `Can_Write()` 之前必须先调用 `CAN_ConfigTxBuff()` 重新激活邮箱**。参考 [Can.c](../../mcu/MCAL/Can/src/Can.c) 中 `Can_Write()` 的实现——第一步就是 `CAN_ConfigTxBuff()`。

**其他可能的 `Can_Write()` 返回值**：
| 返回值 | 含义 |
|--------|------|
| `STATUS_SUCCESS` (0) | 发送成功 |
| `STATUS_BUSY` (2) | 邮箱处于忙状态（未重配） |
| `STATUS_ERROR` (1) | 参数错误（Hth 越界 / PDU 为空 / 未初始化） |
| `STATUS_CAN_BUFF_OUT_OF_RANGE` | 邮箱索引超出硬件范围 |

### 7.2 RX 邮箱"空读" — 没有新数据

**现象**：`Can_Read()` 返回 `STATUS_SUCCESS` 但数据是旧的（重复读同一帧）。

**底层链路**：
```
Can_Read() → CAN_Receive() → FLEXCAN_DRV_Receive() → FLEXCAN_StartRxMessageBufferData()
                                                          │
                                          检查 state->mbs[mb_idx].state != FLEXCAN_MB_IDLE
                                                          │
                                                返回 STATUS_BUSY
```

**注意**：`FLEXCAN_GetMsgBuff()` 本身**不检查 CODE 字段**——它直接读取邮箱 RAM 的 cs + msgId + data 并返回。这意味着即使 CODE = `RX_EMPTY`（没有新帧），它也能"成功"读到旧数据。

**本项目的处理**（[main.c](../../mcu/App/Swc_SignalGateway/src/main.c) 第 57 行）：
```c
if (Can_Read(0, RX_MB, &rx) == STATUS_SUCCESS)
    PINS_DRV_TogglePins(PTD, 1u << 16);  // 只在返回成功时闪灯
```
当前代码只检查返回值，不区分"新帧"还是"重复读旧帧"。如需严格判断，应检查 `cs` 字段的 CODE 部分（bit 24~27）是否为 `0x2`（RX_FULL）。

### 7.3 RX_OVERRUN — 接收丢帧（硬件覆盖）

**现象**：RX 邮箱 CODE 变为 `0x6`（`RX_OVERRUN`），旧帧永久丢失。

**原因**：新帧到达时，RX 邮箱仍处于 `RX_FULL` 状态（CPU 还没来读），硬件被迫覆盖。**这是硬件行为，没有软件能阻止。**

**检测方法**：读取 `cs` 字段 bit 24~27，若为 `0x6` 代表发生过 OVERRUN：
```c
uint8_t code = (rx_msg.cs >> 24) & 0x0F;
if (code == 0x6) {
    // 发生过 OVERRUN，数据已丢失
}
```

**解决**：
- 提高 CPU 轮询/中断频率
- 增加 RX 邮箱数量（分散报文到多个邮箱）
- 使用 FIFO 模式替代邮箱模式（适合高频报文）

### 7.4 多邮箱的 ID 冲突

**现象**：多个 RX 邮箱匹配到同一帧 ID，导致接收行为不确定。

**规则**：FlexCAN 的**邮箱优先级** —— 如果多个 RX 邮箱都能匹配同一帧，编号最小的邮箱优先接收。因此：
- 高频关键帧 → 分配到低编号邮箱
- 低频/掩码范围帧 → 分配到高编号邮箱

### 7.5 发送完成 ≠ 总线上已有应答

**注意**：TX 邮箱 CODE 变为 `TX_INACTIVE` 只代表硬件**已发出**该帧到总线上，**不代表有 ACK 应答**。CAN 总线上必须至少有一个节点在线才能产生 ACK 位——如果单节点测试，帧会反复重发直到进入 Bus-Off 状态。

---

## 8. 收发邮箱满了怎么办？— 完整处理策略

"邮箱满了"的问题分 TX 和 RX 两种情况，根本原因不同，解决途径也不同。

### 8.1 TX 邮箱"满"— 实质是邮箱失活，而非真的满

**问题本质**：TX 邮箱发送完成后 CODE 变为 `TX_INACTIVE`（0x8），`Can_Write()` 返回 `STATUS_BUSY`。这不是"满"，而是**失活**。

**当前项目已规避**（[Can.c](../../mcu/MCAL/Can/src/Can.c) 第 160 行）：
```c
// 每次 Can_Write() 第一步: 重新激活 TX 邮箱
CAN_ConfigTxBuff(&Can_Instance, Hth, &Can_TxBuffCfg);
// 然后才发送
return CAN_Send(&Can_Instance, Hth, &tx_msg);
```
只要照此规范，**单 TX 邮箱不会出现"满"的问题**。

**多 TX 邮箱并行**（密集发送场景）：如果单 TX 邮箱成为瓶颈（发送一帧后需等待 `CAN_ConfigTxBuff` 才能发下一帧），可用多个 TX 邮箱轮转：
```c
static uint8_t tx_round_robin = 0;
status_t Can_Write_RoundRobin(const Can_PduType *pdu) {
    uint8_t mb = tx_round_robin % CAN_TX_COUNT;  // 轮流用不同 TX 邮箱
    CAN_ConfigTxBuff(&Can_Instance, mb, &Can_TxBuffCfg);
    status_t ret = CAN_Send(&Can_Instance, mb, &tx_msg);
    if (ret == STATUS_SUCCESS) tx_round_robin++;
    return ret;
}
```

### 8.2 RX 邮箱"满"— 三种不同层面的"满"

#### 层面 1：硬件 OVERRUN（帧被覆盖，永久丢失）

```
时刻 T1: 帧到达 → CODE: RX_EMPTY → RX_FULL   (帧 A 存入)
时刻 T2: CPU 还没来读，新帧到达
         → CODE: RX_FULL → RX_OVERRUN        (帧 A 被帧 B 覆盖!)
```

**严重性**：⚠️ 帧 A 永久丢失，无法恢复。OVER 发生时甚至没有中断标志提示你。

**检测方法**：
```c
uint8_t code = (rx_msg.cs >> 24) & 0x0F;
if (code == FLEXCAN_RX_OVERRUN) {  // 0x6
    // 检测到 OVERRUN，记录错误
}
```

**恢复方法**：重新配置 RX 邮箱，将 CODE 从 `0x6` 恢复到 `0x4`（RX_EMPTY）。

**预防策略**（按推荐顺序）：

| 优先级 | 策略 | 效果 |
|--------|------|------|
| ⭐⭐⭐ | **中断驱动**替代轮询 | 帧到达 → ISR 立即读取，几乎不会 OVERRUN |
| ⭐⭐ | **增加 RX 邮箱** | 不同 ID 分散到独立邮箱，硬件自动分流 |
| ⭐ | **使用 FIFO 模式** | 队列满时拒绝新帧（旧帧安全），而非覆盖 |

#### 层面 2：CPU 读速跟不上（当前项目的主要风险）

当前 `main.c` 是 500ms 轮询一次。如果对方以 10ms 间隔发帧，500ms 内 50 帧只有最后 1 帧被读到——前 49 帧全部 OVERRUN。

**量化风险**：
```
安全轮询频率 > 1 / (最短帧间隔 × RX 邮箱数)
              = 1 / (10ms × 1) = 100 Hz → 轮询间隔 ≤ 10ms
当前轮询间隔 = 500ms → 理论上只收得到 1/50 的帧
```

**中断驱动的示例**（推荐改造方向）：
```c
// 1. RX 邮箱中断服务函数
void CAN0_ORed_0_15_MB_IRQHandler(void) {
    Can_PduType rx;
    // 快速读取 → 存入软件环形缓冲
    if (Can_Read(0, RX_MB, &rx) == STATUS_SUCCESS) {
        RingBuffer_Push(&can_rx_buf, &rx);
    }
    // 重新锁存 RX 邮箱（CODE 恢复为 RX_EMPTY）
    CAN_ConfigRxBuff(&Can_Instance, RX_MB_INDEX, &Can_RxBuffCfg, rx_id);
}

// 2. 主循环从容消费
void main_loop(void) {
    Can_PduType rx;
    while (RingBuffer_Pop(&can_rx_buf, &rx)) {
        ProcessCanFrame(&rx);  // 慢慢处理，不影响接收
    }
}
```

#### 层面 3：应用层软件缓冲满（当前项目尚未涉及）

如果实现了上述软件环形缓冲（Ring Buffer），当主循环处理慢于 ISR 入队速度时，软件缓冲也会满。需要：
- 增大环形缓冲深度（如 128 帧）
- 添加高水位告警
- 缓冲满时丢弃最旧的帧（或丢弃新帧）

### 8.3 错误处理决策树

```
Can_Write() 返回值:
  ├─ STATUS_SUCCESS (0)        → 发送完成 ✓
  ├─ STATUS_BUSY (2)           → 邮箱失活，重新 CAN_ConfigTxBuff() 后重试
  ├─ STATUS_ERROR (1)          → 检查参数: Hth 是否越界 / PDU 是否为空
  └─ STATUS_CAN_BUFF_OUT_OF_RANGE → 邮箱索引超过硬件最大值

Can_Read() 返回值:
  ├─ STATUS_SUCCESS (0)        → 检查 cs.CODE: 0x2=新帧 / 0x4=旧数据(空读)
  ├─ STATUS_BUSY (2)           → 邮箱被其他操作占用，稍后重试
  └─ STATUS_ERROR (1)          → 检查参数 / 控制器状态
```

---

## 9. 邮箱 vs FIFO — 两种接收模式

FlexCAN 除了邮箱模式，还支持**FIFO（先进先出）模式**，本项目使用的是邮箱模式：

| 特性 | 邮箱（Mailbox） | FIFO |
|------|----------------|------|
| 报文存储 | 每个邮箱存 1 帧 | 一个队列存多帧 |
| ID 过滤 | 每邮箱独立 ID | 全局 ID 表 |
| 溢出保护 | 无（直接覆盖） | 有（队列满时拒绝新帧） |
| CPU 读取 | 随机访问 | 顺序读取 |
| 适用场景 | 固定 ID、需要优先级排序 | 高频同类型报文、流式数据 |
| AUTOSAR 支持 | 基础 | 需要额外配置 |

---

## 10. 总结

```
┌─────────────────────────────────────────────────────────────────┐
│                        核心要点                                  │
├─────────────────────────────────────────────────────────────────┤
│  1. 邮箱是硬件 RAM 区域，不是软件概念                              │
│  2. 每个邮箱存一条 CAN 帧（cs + msgId + data[] + dataLen）       │
│  3. CODE 字段控制邮箱状态：TX_INACTIVE(0x8) → TX_DATA(0xC)        │
│     RX_EMPTY(0x4) → RX_FULL(0x2)                                │
│  4. TX 邮箱每次发送后必须重新配置（CODE 回退到 INACTIVE）           │
│  5. S32K144: CAN0 最多 32 个邮箱，CAN1/CAN2 各 16 个              │
│  6. 本项目: 每通道 16 邮箱（8 TX + 8 RX），实际用了 1 TX + 1 RX    │
│  7. 编号小的邮箱优先级更高                                        │
└─────────────────────────────────────────────────────────────────┘
```
