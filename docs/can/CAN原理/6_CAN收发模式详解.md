# CAN 收发模式详解：轮询、中断与 DMA

> 以本项目 S32K144 + NXP FlexCAN SDK 为基础，对比三种收发模式的原理、API、
> 代码实现、优缺点和适用场景。

---

## 1. 三种模式概览

CAN 报文的收发有三种基本模式，区别在于**谁来通知 CPU "数据到了"**：

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   轮询模式    │     │   中断模式    │     │   DMA 模式    │
│   (Polling)  │     │  (Interrupt) │     │   (仅 RxFIFO) │
├──────────────┤     ├──────────────┤     ├──────────────┤
│              │     │              │     │              │
│  CPU 自己问:  │     │ 硬件通知CPU: │     │ DMA 帮你搬:  │
│  "数据到了?"  │     │ "数据到了!"   │     │ 数据自动从    │
│              │     │              │     │ CAN→内存      │
│  主动轮询     │     │  被动响应     │     │  全自动搬运    │
│              │     │              │     │              │
│  ┌──┐        │     │ ┌──┐ 🔔      │     │ ┌──┐ → DMA → │
│  │CPU│←──────│     │ │CPU│         │     │ │CPU│   ┌──┐  │
│  └──┘ 循环读  │     │ └──┘ 响应    │     │ └──┘ ←─┤RAM│  │
│    ↑         │     │    ↑         │     │    ↑    └──┘  │
│  ┌────┐      │     │  ┌────┐      │     │  ┌────┐       │
│  │CAN │      │     │  │CAN │      │     │  │CAN │       │
│  └────┘      │     │  └────┘      │     │  └────┘       │
└──────────────┘     └──────────────┘     └──────────────┘
```

| | 轮询 | 中断 | DMA |
|--|------|------|-----|
| CPU 参与度 | 全程 | 仅在中断时 | 极低 |
| 实时性 | 差（取决于轮询间隔） | 好（帧到达即响应） | 最好 |
| CPU 开销 | 高（空转消耗） | 中（ISR 上下文切换） | 最低 |
| 实现复杂度 | 最简单 | 中等 | 复杂 |
| 丢帧风险 | 高 | 低（需合理设计） | 最低 |
| 本项目当前使用 | ✅ 在用 | ❌ 未启用 | ❌ 未启用 |

---

## 2. 轮询模式（Polling）— 当前项目的做法

### 2.1 原理

CPU 在主循环中不断检查邮箱状态，"有没有新帧？能不能发送？"——像不断去信箱查看有没有信。

```
主循环:
  while(1) {
      检查 TX 邮箱 → 有空就发
      检查 RX 邮箱 → 有帧就读
      delay_ms(500);        // ← 这个延迟决定了最坏响应时间
      处理其他任务...
  }
```

### 2.2 本项目的轮询实现

来自 [main.c](../../mcu/App/Swc_SignalGateway/src/main.c)：

```c
for(;;)
{
    // ===== 发送 =====
    Can_PduType tx = {.id = 0x123UL, .length = 8U};
    tx.data[0] = cnt & 0xFF;
    // ...
    status_t s = Can_Write(0, TX_MB, &tx);
    if (s != 0) PINS_DRV_ClearPins(PTD, 1u << 1);  // 红灯 = 发送失败

    // ===== 接收 =====
    Can_PduType rx;
    if (Can_Read(0, RX_MB, &rx) == STATUS_SUCCESS)
        PINS_DRV_TogglePins(PTD, 1u << 16);  // 蓝灯闪烁 = 收到帧

    cnt++;
    delay_ms(500);  // ← 500ms 轮询一次
}
```

### 2.3 轮询模式的致命缺陷

```
对方 10ms 发一帧:
  帧1  帧2  帧3  ... 帧50      帧51  帧52  ... 帧100
   ↓    ↓    ↓        ↓         ↓     ↓         ↓
  ───────────────────────────────────────────────────→ 时间
                          ↑                          ↑
                      CPU 读取                    CPU 读取
                    (500ms时)                   (1000ms时)
                    只读到帧50                   只读到帧100
                    帧1~49 全部丢失!             帧51~99 全部丢失!
```

**量化分析**：500ms 轮询间隔，对方 10ms 发一帧 → 50 帧中只读到 1 帧，**丢失率 98%**。

### 2.4 轮询的适用场景

- ✅ 调试/验证阶段（简单直观）
- ✅ 低速通信（1 帧/秒级别）
- ✅ 单节点测试（发一帧等一帧）
- ❌ 量产/实时系统
- ❌ 高频通信
- ❌ 多 ID 同时通信

---

## 3. 中断模式（Interrupt）— 推荐的实时方案

### 3.1 原理

配置好邮箱中断后，硬件在帧到达/发送完成时自动触发中断 → CPU 暂停当前任务 → 跳转到 ISR 执行 → 读/写数据 → 返回继续之前的工作。

```
时间线:
                    ┌─ ISR 读取帧1 ─┐        ┌─ ISR 读取帧2 ─┐
                    │               │        │               │
  主循环:  ████████████░░░░░░███████████████████░░░░░░███████████
           │          │      │                    │      │
           │          │      └─ 继续主循环         │      └─ 继续
           │          │                            │
  硬件:    └── 帧1到达，触发 IRQ                    └── 帧2到达，触发 IRQ
```

### 3.2 S32K144 的 CAN 中断向量

来自 [S32K144.h](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/include/S32K144.h) ：

| 中断向量 | IRQ 号 | 触发条件 |
|----------|--------|----------|
| `CAN0_ORed_0_15_MB_IRQn` | 81 | MB 0~15 中任何一个完成收发 |
| `CAN0_ORed_16_31_MB_IRQn` | 82 | MB 16~31 中任何一个完成收发 |
| `CAN0_ORed_IRQn` | 78 | Bus Off / TX Warning / RX Warning |
| `CAN0_Error_IRQn` | 79 | CAN 总线错误 |
| `CAN0_Wake_Up_IRQn` | 80 | Pretended Networking 唤醒 |

**注意**：0~15 号邮箱共用一个中断线（OR'ed）。ISR 被触发后，需要轮询 `IFLAG` 寄存器找出具体是哪个 MB 产生的中断。

### 3.3 SDK 中的中断 API

来自 [flexcan_driver.h](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/drivers/inc/flexcan_driver.h) ：

```c
// 回调函数类型
typedef void (*flexcan_callback_t)(uint8_t instance,
    flexcan_event_type_t eventType,
    uint32_t buffIdx,
    flexcan_state_t *flexcanState);

// 安装事件回调（在 ISR 内部被调用）
void FLEXCAN_DRV_InstallEventCallback(uint8_t instance,
                                       flexcan_callback_t callback,
                                       void *callbackParam);

// 安装错误回调
void FLEXCAN_DRV_InstallErrorCallback(uint8_t instance,
                                       flexcan_error_callback_t callback,
                                       void *callbackParam);
```

**SDK 回调事件类型**（`flexcan_event_type_t`）：

| 事件 | 含义 |
|------|------|
| `FLEXCAN_EVENT_RX_COMPLETE` | RX 邮箱收到一帧 |
| `FLEXCAN_EVENT_TX_COMPLETE` | TX 邮箱发送完成 |
| `FLEXCAN_EVENT_RXFIFO_COMPLETE` | RxFIFO 收到一帧 |
| `FLEXCAN_EVENT_RXFIFO_WARNING` | RxFIFO 快满（还剩 5 帧空间） |
| `FLEXCAN_EVENT_RXFIFO_OVERFLOW` | RxFIFO 已满，丢了一帧 |
| `FLEXCAN_EVENT_ERROR` | 总线错误 |

### 3.4 ISR 内部流程

SDK 已实现了 ISR 框架（来自 [flexcan_driver.c](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/drivers/src/flexcan/flexcan_driver.c) 的 `FLEXCAN_IRQHandler()`）：

```
FLEXCAN_IRQHandler(instance)
  │
  ├─ 遍历 IFLAG 寄存器，找到产生中断的 MB
  │
  ├─ If MB 是 RxFIFO 溢出中断:
  │   └─ 调 FLEXCAN_EVENT_RXFIFO_OVERFLOW 回调
  │
  ├─ If MB 是 RX 邮箱 (state == FLEXCAN_MB_RX_BUSY):
  │   ├─ FLEXCAN_IRQHandlerRxMB()  // 锁存 MB → 读数据 → 解锁
  │   ├─ 清除中断标志
  │   ├─ 调 callback(FLEXCAN_EVENT_RX_COMPLETE, mb_idx)
  │   └─ FLEXCAN_CompleteTransfer()  // 信号量释放（阻塞模式）
  │
  ├─ If MB 是 TX 邮箱 (state == FLEXCAN_MB_TX_BUSY):
  │   ├─ 清除中断标志
  │   ├─ 调 callback(FLEXCAN_EVENT_TX_COMPLETE, mb_idx)
  │   └─ FLEXCAN_CompleteTransfer()  // 信号量释放（阻塞模式）
  │
  └─ 清除未预期中断标志（防死循环）
```

### 3.5 中断模式示例代码

```c
#include "flexcan_driver.h"

/* ===== 环形缓冲（ISR → 主循环解耦）===== */
#define RX_BUF_SIZE 64
static Can_PduType rx_ring[RX_BUF_SIZE];
static volatile uint8_t rx_head = 0;  // ISR 写入位置
static volatile uint8_t rx_tail = 0;  // 主循环读取位置

/* ===== 事件回调（在 ISR 上下文中执行）===== */
static void Can_EventCallback(uint8_t instance,
                               flexcan_event_type_t eventType,
                               uint32_t buffIdx,
                               flexcan_state_t *state)
{
    if (eventType == FLEXCAN_EVENT_RX_COMPLETE) {
        /* 从邮箱读取帧 → 立即存入环形缓冲 */
        Can_PduType *slot = &rx_ring[rx_head];
        // 读取邮箱数据（idx = num_tx + buffIdx）
        CAN_Receive(&Can_Instance, num_tx_mb + buffIdx, &rx_msg);
        slot->id     = rx_msg.id;
        slot->length = rx_msg.length;
        memcpy(slot->data, rx_msg.data, rx_msg.length);

        rx_head = (rx_head + 1) % RX_BUF_SIZE;
        if (rx_head == rx_tail) {
            /* 环形缓冲满！丢弃最旧的帧 */
            rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        }

        /* 重新激活 RX 邮箱（CODE: FULL→EMPTY） */
        CAN_ConfigRxBuff(&Can_Instance, num_tx_mb + buffIdx,
                         &Can_RxBuffCfg, rx_mailboxes[buffIdx].id);
    }

    if (eventType == FLEXCAN_EVENT_TX_COMPLETE) {
        /* 发送完成，可以准备下一帧 */
        tx_busy = false;
    }
}

/* ===== 主循环：从容消费环形缓冲 ===== */
void main_loop(void)
{
    while (1) {
        /* 处理所有待处理的接收帧 */
        while (rx_tail != rx_head) {
            Can_PduType *frame = &rx_ring[rx_tail];
            ProcessCanFrame(frame);  // 应用层处理
            rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        }

        /* 执行其他任务... */
        OtherTasks();
    }
}
```

### 3.6 中断模式的优缺点

| 优势 | 劣势 |
|------|------|
| ✅ 实时响应，帧到达立即读取 | ⚠️ ISR 中必须快速完成（不能阻塞） |
| ✅ CPU 不用空转轮询 | ⚠️ 中断上下文不能做复杂处理 |
| ✅ 丢帧风险大大降低 | ⚠️ 需要环形缓冲解耦 ISR 和主循环 |
| ✅ 适合多帧、多 ID 场景 | ⚠️ 需合理设计临界区保护 |

---

## 4. 阻塞 vs 非阻塞 — SDK API 的两个子模式

SDK 在中端驱动之上，还封装了**阻塞**和**非阻塞**两种 API 风格，适用于不同场景。

### 4.1 对比

| | 阻塞 (Blocking) | 非阻塞 (Non-blocking) |
|--|-----------------|-----------------------|
| **API** | `FLEXCAN_DRV_SendBlocking()` | `FLEXCAN_DRV_Send()` |
| | `FLEXCAN_DRV_ReceiveBlocking()` | `FLEXCAN_DRV_Receive()` |
| | `FLEXCAN_DRV_RxFifoBlocking()` | `FLEXCAN_DRV_RxFifo()` |
| **行为** | 调用后卡住，直到完成或超时 | 调用后立即返回 |
| **等待机制** | 信号量 (semaphore) | 回调函数 |
| **超时** | 可设置超时 (ms) | 不适用 |
| **适用场景** | 简单同步流程 | 实时多任务系统 |

### 4.2 阻塞模式：一个函数等到完成

```c
// 发送: 阻塞直到发完或超时
flexcan_data_info_t tx_info = {
    .msg_id_type = FLEXCAN_MSG_ID_STD,
    .data_length = 8,
    .is_remote   = false,
};
status_t ret = FLEXCAN_DRV_SendBlocking(
    0,               // CAN 实例
    TX_MB,           // 邮箱索引
    &tx_info,
    0x123,           // CAN ID
    tx_data,         // 数据
    100U             // 超时 100ms
);
if (ret == STATUS_SUCCESS) {
    // 发送成功
} else if (ret == STATUS_TIMEOUT) {
    // 超时 —— 总线上可能没有 ACK 节点
}

// 接收: 阻塞直到收到帧或超时
flexcan_msgbuff_t rx_mb;
ret = FLEXCAN_DRV_ReceiveBlocking(0, RX_MB, &rx_mb, 500U);
if (ret == STATUS_SUCCESS) {
    // rx_mb.data[] 中是收到的数据
}
```

**内部机制**（来自 `flexcan_mb_handle_t`）：
```
调用 SendBlocking()
  → 发送数据
  → 等待信号量 mbSema（内部 semaphore）
  → ISR 完成发送后释放信号量
  → 函数返回 STATUS_SUCCESS
  → 或超时返回 STATUS_TIMEOUT
```

### 4.3 非阻塞模式：发完就回调

```c
// 安装回调
FLEXCAN_DRV_InstallEventCallback(0, MyCallback, NULL);

// 发送（立即返回）
status_t ret = FLEXCAN_DRV_Send(0, TX_MB, &tx_info, 0x123, tx_data);
// ret == STATUS_SUCCESS 只代表发射成功，不代表发送完成

// 几毫秒后，ISR 触发 → MyCallback(FLEXCAN_EVENT_TX_COMPLETE, ...)
```

### 4.4 返回值速查

| 返回值 | 含义 |
|--------|------|
| `STATUS_SUCCESS` (0) | 操作成功发起 |
| `STATUS_BUSY` (2) | 邮箱忙（被上次操作占用） |
| `STATUS_TIMEOUT` (3) | 阻塞调用超时 |
| `STATUS_ERROR` (1) | 参数/状态错误 |
| `STATUS_CAN_BUFF_OUT_OF_RANGE` | 邮箱索引超出硬件范围 |

---

## 5. DMA 模式 — 零 CPU 参与的接收

### 5.1 原理

DMA（Direct Memory Access）是一个独立的硬件模块，可以不经过 CPU，直接把 CAN 控制器的数据搬运到内存中。

```
轮询/中断模式:                        DMA 模式:
  CAN → CPU 读 → 写 RAM              CAN → DMA → RAM
       ↑   ↑                              ↑   ↑
       手动搬运                            全自动
```

**限制**：S32K144 的 FlexCAN DMA **仅支持 RxFIFO**，不支持单个邮箱的 DMA。所以 DMA 模式需要先启用 RxFIFO。

### 5.2 S32K144 DMA 支持

来自 [S32K144_features.h](../../mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/include/S32K144_features.h)：

```c
#define FEATURE_CAN_HAS_DMA_ENABLE  (1)     // 硬件支持 DMA
```

DMA 通道请求：
```c
#define FEATURE_CAN_EDMA_REQUESTS  { EDMA_REQ_FLEXCAN0, \
                                     EDMA_REQ_FLEXCAN1, \
                                     EDMA_REQ_FLEXCAN2 }
```

### 5.3 DMA 模式配置

```c
flexcan_user_config_t canConfig;
FLEXCAN_DRV_GetDefaultConfig(&canConfig);

/* 启用 RxFIFO */
canConfig.is_rx_fifo_needed = true;
canConfig.num_id_filters   = FLEXCAN_RX_FIFO_ID_FILTERS_8;

/* 选择 DMA 传输方式 */
canConfig.transfer_type    = FLEXCAN_RXFIFO_USING_DMA;
canConfig.rxFifoDMAChannel = 0;   // DMA 通道 0

FLEXCAN_DRV_Init(0, &canState, &canConfig);

/* 配置 RxFIFO ID 过滤表 */
flexcan_id_table_t table[] = {
    {.isRemoteFrame = false, .isExtendedFrame = false, .id = 0x100},
    {.isRemoteFrame = false, .isExtendedFrame = false, .id = 0x200},
};
FLEXCAN_DRV_ConfigRxFifo(0, FLEXCAN_RX_FIFO_ID_FORMAT_A, table);

/* 安装 DMA 完成回调 */
FLEXCAN_DRV_InstallEventCallback(0, DmaCompleteCallback, NULL);
```

### 5.4 DMA 模式对比

| | 中断模式 | DMA 模式 |
|--|---------|----------|
| RX 方式 | 仅 RxFIFO 支持 | 仅 RxFIFO 支持 |
| CPU 开销 | 每帧进一次 ISR | 一批帧才进一次 ISR |
| 延迟 | 微秒级 | 微秒级（略高于中断） |
| 适用速率 | 中高频 | 极高频（如 CAN FD 满载） |
| 复杂度 | 中 | 高（需配 DMA 通道、描述符） |
| 本项目需求 | 足够 | 过度设计 |

---

## 6. 三种模式的完整对比

```
  发送侧:                             接收侧:

  ┌─────────────────────────┐        ┌─────────────────────────┐
  │  模式        API        │        │  模式        API        │
  ├─────────────────────────┤        ├─────────────────────────┤
  │  轮询        Can_Write  │        │  轮询        Can_Read   │
  │              (每次重配)  │        │              (返回即读)  │
  ├─────────────────────────┤        ├─────────────────────────┤
  │  中断/阻塞   SendBlocking│       │  中断/阻塞   ReceiveBlocking│
  │              (等信号量)  │        │              (等信号量)  │
  ├─────────────────────────┤        ├─────────────────────────┤
  │  中断/非阻塞 Send       │        │  中断/非阻塞 Receive    │
  │  (中断模式)  (回调通知)  │        │  (中断模式)  (回调通知)  │
  ├─────────────────────────┤        ├─────────────────────────┤
  │              N/A        │        │  DMA         RxFIFO     │
  │              (TX 无 DMA) │        │              (全自动)    │
  └─────────────────────────┘        └─────────────────────────┘
```

| 维度 | 轮询 | 中断（非阻塞） | 中断（阻塞） | DMA |
|------|------|:-------------:|:-----------:|:---:|
| CPU 占用 | 高 | 低 | 中（卡在信号量） | 极低 |
| 响应延迟 | 取决于轮询间隔 | 微秒级 | 微秒 + 超时等待 | 微秒级 |
| 实现难度 | ★☆☆☆ | ★★★☆ | ★★☆☆ | ★★★★ |
| 丢帧风险 | 高 | 低（需环形缓冲） | 低 | 极低 |
| 多任务友好 | 否 | 是 | 否（阻塞） | 是 |
| 适用场景 | 调试/低速 | 实时系统 | 简单同步 | 极高频 FD |

---

## 7. 本项目的收发模式现状与改进建议

### 7.1 当前状态

```
当前实现:
  SDK 层:  ISR 框架已注册（flexcan_irq.c 已将向量表映射到 FLEXCAN_IRQHandler），
           FLEXCAN_DRV_Init() 会调用 FLEXCAN_EnableIRQs() 使能 NVIC 中断
  PAL 层:  CAN_Init() 固定使用 FLEXCAN_RXFIFO_USING_INTERRUPTS（不用 DMA）
  MCAL 层 (Can.c): 未调用 CAN_InstallEventCallback() → 中断虽然触发但无用户回调
  APP 层 (main.c): delay_ms(500) + 轮询 Can_Read/Can_Write
  MCAL 配置: Can_Cfg.h 定义了 CAN_INTERRUPT_ENABLE = STD_ON 但 Can.c 并未使用

本质: 硬件中断已就绪，但软件层未安装回调 → 实际等价于纯轮询，500ms 读一次
```

### 7.2 最小改动：在 MCAL 层安装中断回调

SDK 已注册好 ISR 向量表，只需在 `Can_Init()` 中安装回调即可激活中断接收：

```c
// 在 Can.c 的 Can_Init() 末尾添加:
CAN_InstallEventCallback(&Can_Instance, Can_McalEventCallback, NULL);
```

然后实现回调（在 ISR 上下文中执行，需快速返回）：
```c
static void Can_McalEventCallback(uint8_t instance, can_event_t event,
                                   uint32_t buffIdx, void *state)
{
    if (event == CAN_EVENT_RX_COMPLETE) {
        can_message_t rx_msg;
        CAN_Receive(&Can_Instance, buffIdx, &rx_msg);
        // 存入环形缓冲，不在 ISR 中做复杂处理
        RingBuffer_Push(&rx_ring, &rx_msg);
        // 重锁存 RX 邮箱
        CAN_ConfigRxBuff(&Can_Instance, buffIdx, &Can_RxBuffCfg, rx_id);
    }
}
```

### 7.3 推荐方案：MCAL 层添加中断支持

在 `Can.c` 中增加：

```
1. 环形缓冲实现（rx_ring_buffer.h/c）
2. RX 中断回调 → Can_RxIndication（标准 AUTOSAR 接口）
3. Can_MainFunctionRead() — 主循环消费函数
4. TX 中断回调 → Can_TxConfirmation
```

### 7.5 量产车的模式选择

| 模式 | 量产车占比 | 典型应用场景 |
|------|:--------:|------------|
| 中断（非阻塞 + 回调） | **~90%** | 实时帧：CAN NM、诊断响应、XCP 标定、关键传感器/执行器信号 |
| 中断（阻塞 + 信号量） | ~5% | 简单 ECU：启动时的配置握手、诊断会话切换 |
| 轮询（MainFunction） | ~5% | 非实时帧：配合中断使用，周期读取常规状态数据 |
| DMA | **几乎为 0** | 极少数 CAN FD 满载网关 / Black Box 数据记录器 |

**AUTOSAR CP 标准明确规定了两条接收路径，量产 ECU 通常两条同时用：**

```
┌──────────────────────────────────────────────────────┐
│                 AUTOSAR Can 模块                       │
│                                                      │
│  中断路径（实时数据）:                                   │
│    CAN 帧到达 → ISR → Can_RxIndication() →            │
│    CanIf_RxIndication() → PduR → COM                  │
│    用途: 网络管理、诊断 Tester Present、               │
│          故障响应、关键控制信号                          │
│                                                      │
│  轮询路径（非实时数据）:                                  │
│    OS 周期任务 → Can_MainFunctionRead() →             │
│    CanIf_RxIndication() → PduR → COM                  │
│    用途: 常规传感器数据、周期性状态上报、                  │
│          不紧急的批量 I-PDU                             │
│                                                      │
│  两条路径并存，中断保证实时性，轮询降低中断频率              │
└──────────────────────────────────────────────────────┘
```

**为什么 DMA 在量产车上几乎不用？**

S32K144 的 Cortex-M4 跑 48MHz，CAN 500kbps 最多约每秒 5000 帧。每次 ISR 几十微秒，CPU 占用率不到 1%——省下来的 CPU 时间也没地方用。而 DMA 引入的异步调试难度、时序不确定性、以及 ISO 26262 功能安全认证的额外成本，远超过省那 1% CPU 的收益。

> 你的 SDK PAL 层 `CAN_Init()` 固定写死 `transfer_type = FLEXCAN_RXFIFO_USING_INTERRUPTS`，也从侧面印证了 NXP 官方也不推荐用 DMA 做常规 CAN 通信。

---

## 8. 总结

```
┌──────────────────────────────────────────────────────────────────┐
│                          核心要点                                 │
├──────────────────────────────────────────────────────────────────┤
│  1. 轮询 = CPU 主动问，简单但实时性差、丢帧风险高                     │
│  2. 中断 = 硬件通知 CPU，实时性好、需要 ISR + 环形缓冲解耦            │
│  3. DMA  = 硬件自动搬运数据到内存，CPU 零参与，仅 RxFIFO 支持          │
│  4. 同一 ISR 内又有阻塞(等信号量) vs 非阻塞(回调)之分               │
│  5. 量产车: 中断是绝对主流(~90%)，DMA 基本不用                       │
│  6. AUTOSAR 标准: 中断(实时帧) + 轮询 MainFunction(非实时帧) 并存    │
│  7. 本项目: SDK 已注册 ISR 向量表，MCAL 未安装回调 → 等同纯轮询        │
│  8. 最小改动: Can_Init() 中加一行 CAN_InstallEventCallback() 即可      │
│  9. 中断 ISR 的核心原则: 尽可能快 → 只做拷贝 + 重锁存邮箱              │
└──────────────────────────────────────────────────────────────────┘
```

