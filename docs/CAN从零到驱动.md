# CAN 从零到驱动 —— S32K144 FlexCAN 教学文档

> **目标读者**：有 Linux C++ 基础、没写过 MCU 驱动、不知道 CAN 是什么的开发者
> **配套项目**：Domain_Controller `mcu/src/flexcan.c` + `mcu/include/flexcan.h`
> **学习方式**：按顺序读完各篇后，对照本文 SDK API 章节在 flexcan.c 中使用 NXP S32 SDK API
> **核心原则**：只讲"为什么"和"怎么做"，全程用表格 + 伪代码 + 大白话
> **开发方式**：本工程使用 NXP S32 SDK API 开发，不涉及直接寄存器操作

---

## 关于 SDK

本工程的 MCU 代码基于 **NXP S32 SDK for S32K1xx (RTM 4.0.2)** 开发。
SDK 位于 `mcu/S32_SDK_S32K1xx_RTM_4.0.2/`。

### 开发方式：使用 SDK API，不操作寄存器

| 概念理解（本文学） | 工程实现（SDK 方式） |
|---------|---------|
| CAN 协议、帧结构、仲裁、位时序理解 | SDK API：`FLEXCAN_DRV_Init()`、`FLEXCAN_DRV_SendBlocking()` 等 |
| FlexCAN 模块功能、MB 工作流程、波特率计算 | SDK 结构体：`flexcan_user_config_t`、`flexcan_time_segment_t` |
| 初始化流程步骤（使能时钟→冻结→配置→退出） | 一句话初始化：`FLEXCAN_DRV_Init(0, &state, &config)` |

第一遍学 CAN 时理解概念；实际工程代码中直接调用 SDK API，SDK 内部完成了所有寄存器操作。

### SDK 头文件路径速查

| 模块 | SDK 头文件（相对 SDK 根目录） | 常用 API 前缀 |
|------|------------------------------|--------------|
| FlexCAN | `platform/drivers/inc/flexcan_driver.h` | `FLEXCAN_DRV_*` |
| UART (LPUART) | `platform/drivers/inc/lpuart_driver.h` | `LPUART_DRV_*` |
| GPIO/Pins | `platform/drivers/inc/pins_driver.h` | `PINS_DRV_*` |
| Timer (LPIT) | `platform/drivers/inc/lpit_driver.h` | `LPIT_DRV_*` |
| ADC | `platform/drivers/inc/adc_driver.h` | `ADC_DRV_*` |
| Clock | `platform/drivers/inc/clock_manager.h` | `CLOCK_SYS_*` |

所有头文件的物理路径为 `mcu/S32_SDK_S32K1xx_RTM_4.0.2/` + 上述相对路径，
在 Makefile 中通过 `-I` 添加包含路径。

---

## 第 0 篇：写在最前——MCU 与外设交互 {#p0}

### 0.1 外设与 API

MCU 芯片内部集成了 CPU、内存和各种外设模块（CAN、UART、ADC、定时器等）。
每个外设模块通过一组控制/状态/数据寄存器与 CPU 交互——往这些地址写特定值，
等于在给外设发指令；读这些地址，等于在查询外设的状态。

NXP S32 SDK 将这些寄存器操作封装成易用的 API 和配置结构体，开发者不需要
直接面对寄存器的地址和位布局：

```c
/* SDK API 方式（本工程采用）—— 不需知道寄存器地址 */
FLEXCAN_DRV_Init(0, &state, &config);

/* 传统寄存器方式（本工程不采用）—— 需要知道每个位的位置
   regs->MCR = (1 << 30) | (1 << 28) | ...; */
```

### 0.2 volatile 关键字

编译器有时会优化掉对同一地址的重复访问。对于外设寄存器，每次读/写都有
实际的硬件效果，不能被优化掉。SDK 在头文件中已经包含了 `volatile` 声明，
开发者直接使用 API 时无需关心。

### 0.3 SDK 配置结构体 + API

SDK 外设驱动的典型使用模式：

```c
/* 1. 声明配置结构体 */
flexcan_user_config_t config;

/* 2. 获取默认配置（SDK 填充合理的默认值） */
FLEXCAN_DRV_GetDefaultConfig(&config);

/* 3. 按需修改配置字段 */
config.max_num_mb = 3;       /* 只使用 3 个消息缓冲区 */
config.bitrate.bitrate = 500000;  /* 500kbps */

/* 4. 声明驱动状态变量（必须全局/静态，驱动内部会使用） */
static flexcan_state_t g_flexState;

/* 5. 调用初始化 API —— SDK 内部完成所有寄存器配置序列 */
status_t ret = FLEXCAN_DRV_Init(0, &g_flexState, &config);
```

---

## 第 1 篇：CAN 是什么？ {#p1}

### 1.1 为什么有 CAN

一辆车里有几十个电子控制器（ECU）：发动机 ECU、变速箱 ECU、ABS ECU、
仪表盘、车身控制器。如果每两个 ECU 之间点对点连线，线束会又重又贵又难修。

**CAN = Controller Area Network（控制器局域网）**：所有 ECU 挂在一对总线上，
谁想发数据就发，大家都听得见。

```
     ┌─────┐   ┌─────┐   ┌─────┐   ┌─────┐
     │发动机│   │ABS  │   │仪表盘│   │变速箱│
     └──┬──┘   └──┬──┘   └──┬──┘   └──┬──┘
        │         │         │         │
  ──────┴─────────┴─────────┴─────────┴────── CAN_H
  ──────┬─────────┬─────────┬─────────┬────── CAN_L
        │         │         │         │
     120Ω                              120Ω
     (终端电阻)                       (终端电阻)
```

### 1.2 差分信号

CAN 用两根线（CAN_H / CAN_L）的电压差表示逻辑值：

| 状态 | CAN_H 电压 | CAN_L 电压 | 差值 | 逻辑 |
|------|-----------|-----------|------|------|
| 显性 (Dominant) | 3.5V | 1.5V | 2V | **0** |
| 隐性 (Recessive) | 2.5V | 2.5V | 0V | **1** |

差分信号抗干扰能力强：两根线同时受电磁干扰，差值基本不变。

### 1.3 终端电阻

总线两端各接一个 **120Ω 电阻**，吸收信号防止反射。不接终端电阻会导致数据错乱。

### 1.4 CAN 帧结构（标准数据帧）

```
┌───┬────┬───┬───┬────┬────────────┬──────┬────┬────┬────┐
│ S │ ID │RTR│IDE│ r0 │    DLC     │ DATA │CRC │ACK │ EOF│
│ O │    │   │   │    │  (4bit)    │0~8字节│ 15 │  2 │ 7  │
│ F │11位│ 1 │ 1 │ 1  │  数据长度  │      │ bit│ bit│ bit│
└───┴────┴───┴───┴────┴────────────┴──────┴────┴────┴────┘
```

| 字段 | 说明 |
|------|------|
| **SOF** (Start of Frame) | 帧开始标志（显性位 0） |
| **ID** (Identifier) | 11 位（标准）或 29 位（扩展），决定优先级 |
| **RTR** | 0=数据帧，1=远程帧 |
| **IDE** | 0=标准帧，1=扩展帧 |
| **DLC** (Data Length Code) | 数据场有几个字节（0~8） |
| **DATA** | 0~8 字节数据 |
| **CRC** | 15 位循环冗余校验 |
| **ACK** | 接收方回复"收到了" |
| **EOF** (End of Frame) | 7 个隐性位，帧结束 |

### 1.5 标准帧 vs 扩展帧

| | 标准帧 | 扩展帧 |
|--|--------|--------|
| ID 位数 | 11 位 | 29 位 |
| ID 范围 | 0x000~0x7FF | 0x00000000~0x1FFFFFFF |
| 帧长度 | 较短 | 较长 |
| 常用场景 | 动力总成（ECU、ABS） | 诊断通信 (UDS) |

### 1.6 仲裁机制

多个节点同时发送时，**ID 值最小的节点获得总线控制权**：
- 双方同时发 ID 的每一位
- 显性位（0）压倒隐性位（1）
- 发送 1 但总线为 0 的节点 → 仲裁失败 → 停止发送，转为接收

### 1.7 位填充 (Bit Stuffing)

发送方在连续 5 个相同位后，强制插入一个相反位，防止接收方失去同步。

---

## 第 2 篇：CAN 位时序 (Bit Timing) {#p2}

### 2.1 bit 内部的时间分割

CAN 把一个 bit 的持续时间分成若干 **时间量子 (Time Quantum, TQ)**：

```
    ┌────┬──────────┬──────────┬──────────┐
    │SYNC│ PROP_SEG │PHASE_SEG1│PHASE_SEG2│
    │SEG │  (1~8)   │  (1~8)   │  (1~8)   │ ← TQ 数
    │ 1  │          │          │          │
    └────┴──────────┴──────────┴─────┬────┘
                                      │
                                   采样点
```

| 段 | TQ 数 | 作用 |
|----|-------|------|
| SYNC_SEG | 固定 1 TQ | 信号跳变沿同步 |
| PROP_SEG | 1~8 TQ | 补偿总线传输延时 |
| PHASE_SEG1 | 1~8 TQ | 采样点在此段末尾 |
| PHASE_SEG2 | 1~8 TQ | 采样后的时间 |

### 2.2 采样点

接收方在 PHASE_SEG1 和 PHASE_SEG2 之间读取总线电平，这就是判决
"这一位是 0 还是 1" 的时刻。一般推荐采样点在 **70%~80%** 位置。

### 2.3 波特率公式

```
波特率 (bps) = F_can_clk / ((PRESDIV + 1) × 总 TQ 数)
```

S32K144 FlexCAN 默认选择外设时钟（PER_CLK=80MHz）：

```
实例：80MHz 时钟配 500kbps
总 TQ 数 = 1 (SYNC) + 7 (PROP) + 4 (PSEG1) + 2 (PSEG2) = 16
PRESDIV = 80,000,000 / (500,000 × 16) - 1 = 9
验算：80MHz / ((9+1) × 16) = 500kbps ✓
```

### 2.4 常用波特率参数（外设时钟 80MHz）

| 波特率 | PRESDIV | PROP_SEG | PSEG1 | PSEG2 | 总 TQ | 采样点 |
|-------|---------|----------|-------|-------|-------|--------|
| 500k | 9 | 7 | 4 | 2 | 16 | 75% |
| 250k | 19 | 7 | 4 | 2 | 16 | 75% |
| 125k | 39 | 7 | 4 | 2 | 16 | 75% |

### 2.5 SDK 中的位时序配置

不需要手动计算寄存器值，SDK 提供结构体字段和默认配置：

```c
/* 方式一：用默认配置（500kbps） */
flexcan_user_config_t config;
FLEXCAN_DRV_GetDefaultConfig(&config);
/* config.bitrate 自动设为 500kbps 参数 */

/* 方式二：手动设置波特率 */
config.bitrate = 250000;   /* 改为 250kbps */
```

SDK 内部根据配置自动计算寄存器值。

---

## 第 3 篇：FlexCAN 模块概念与 SDK 使用 {#p3}

### 3.1 FlexCAN 是什么

FlexCAN 是 S32K144 内部的 CAN 控制器，负责生成/解析符合 CAN 2.0B 协议
的比特流。它**不是** CAN 收发器——收发器是外部芯片，负责将逻辑电平转换成
CAN_H/CAN_L 差分信号。

```
S32K144 芯片内部             芯片外部
┌──────────────────────┐    ┌────────┐
│  CPU                 │    │ CAN    │
│    ↓                 │───→│ 收发器 │──→ CAN_H
│  系统总线            │←───│ (外部) │──→ CAN_L
│    ↓                 │    └────────┘
│  FlexCAN 模块 ←→ MB[]│
└──────────────────────┘
```

### 3.2 消息缓冲区 (Message Buffer, MB)

FlexCAN 内部有若干个 **消息缓冲区 (MB)**，每个 MB 存储一个 CAN 帧。
可以把 MB 想象成"收发室的信箱"：

```
FlexCAN 模块
┌──────────────────────────────────────────────┐
│ MB0   MB1   MB2   MB3   MB4 ...  MB15       │
│ [16B] [16B] [16B] [16B] [16B]      [16B]    │
└──────────────────────────────────────────────┘
```

- **发送**：把数据写进 MB，通知硬件发送
- **接收**：硬件自动匹配 ID，存入对应 MB
- S32K144 FlexCAN 有 16 个 MB（MB0~MB15）

**SDK 中的 MB 管理**：SDK 内部自动管理 MB 的分配和状态，开发者只需在
初始化时指定 MB 数量，收发时通过 `mb_idx` 参数指定使用哪个 MB。

```c
/* SDK 自动管理 MB */
flexcan_user_config_t config;
config.max_num_mb = 3;                      /* 只用 3 个 MB */
FLEXCAN_DRV_Init(0, &g_state, &config);    /* SDK 自动配置 MB */

/* 发送 */
FLEXCAN_DRV_SendBlocking(0, 0, &txInfo, id, data, timeout);  /* MB0 发送 */

/* 配置接收（按需指定接收过滤） */
FLEXCAN_DRV_ConfigRxMb(0, 1, &rxInfo, 0);  /* MB1 接收任意 ID */
```

**每个 MB 的 16 字节数据结构**（理解即可，SDK 用 `flexcan_msgbuff_t` 封装）：

| 偏移 | 内容 |
|------|------|
| +0x00 | CS（状态码 + IDE + RTR + DLC + 时间戳） |
| +0x04 | ID（11 位或 29 位帧 ID） |
| +0x08 | DATA[3:0]（数据低 4 字节） |
| +0x0C | DATA[7:4]（数据高 4 字节） |

SDK 用 `flexcan_msgbuff_t` 封装了数据读写，无需手动操作：

```c
/* 收到的帧数据在 rxMsgBuff 中 */
uint32_t rxId   = rxMsgBuff.msgId;          /* CAN ID */
uint8_t  rxLen  = rxMsgBuff.dataLen;        /* 数据长度 */
uint8_t *rxData = rxMsgBuff.data;           /* 数据指针 */
```

### 3.3 CODE 值（MB 状态机）

每个 MB 的 CODE 字段表示当前状态。理解这些状态有助于排查问题：

| CODE | 发送角色 | 接收角色 |
|------|---------|---------|
| 0x04 | - | RX_EMPTY：等待接收 |
| 0x08 | TX_INACTIVE：空闲可发 | - |
| 0x0C | TX_ACTIVE：正在发送 | RX_FULL：收到新帧 |
| 0x0A | - | RX_OVERWRITE：被覆盖 |

SDK 收发 API 内部自动管理 CODE 转换，无需手动操作。

---

## 第 4 篇：初始化流程（SDK 方式） {#p4}

### 4.1 一句话初始化

```c
#include "flexcan_driver.h"

static flexcan_state_t g_flexState;   /* 驱动状态（必须全局/静态） */

status_t can_init(void)
{
    flexcan_user_config_t config;

    /* 1. 获取 SDK 默认配置（500kbps, 16 MB） */
    FLEXCAN_DRV_GetDefaultConfig(&config);

    /* 2. 按需自定义 */
    config.max_num_mb = 3;                      /* 只用 3 个 MB */
    config.is_rx_fifo_needed = false;           /* 不用 FIFO */
    config.flexcanMode = FLEXCAN_NORMAL_MODE;   /* 普通模式 */

    /* 3. ★ 一句话初始化（内部自动完成所有寄存器配置） */
    return FLEXCAN_DRV_Init(0, &g_flexState, &config);
}
```

### 4.2 SDK 内部做的事情

`FLEXCAN_DRV_Init()` 内部分步完成了以下所有操作：

1. ✅ 使能 PCC 外设时钟（没时钟，写任何寄存器都无效）
2. ✅ 等待冻结确认（FRZ=1）
3. ✅ 软复位（所有寄存器恢复默认值）
4. ✅ 配置 MCR 模块配置寄存器（MAXMB、FRZ、HALT 等）
5. ✅ 配置 CTRL1 控制寄存器（位时序参数，根据 config.bitrate 计算）
6. ✅ 清空所有 MB
7. ✅ 根据 MB 数配置 MB 区域
8. ✅ 配置中断/轮询模式
9. ✅ 退出冻结（HALT=0）
10. ✅ 等待模块正常运行

### 4.3 参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| `instance` | `uint8_t` | CAN 实例号：0=CAN0, 1=CAN1 |
| `state` | `flexcan_state_t *` | 驱动状态变量（全局/静态，内部使用） |
| `config` | `const flexcan_user_config_t *` | 配置结构体 |

### 4.4 错误排查对照

| 现象 | 常见原因 |
|------|---------|
| 初始化返回超时 | FlexCAN 时钟未使能或硬件故障 |
| 能发不能收 | 未配置接收 MB 或 MB 未设为 RX_EMPTY |
| 发送总是超时 | 波特率不匹配或总线无终端电阻 |
| 收到的数据错乱 | 波特率不一致或总线干扰 |

---

## 第 5 篇：发送流程（SDK 方式） {#p5}

### 5.1 阻塞发送

```c
#include "flexcan_driver.h"

/* 准备数据 */
uint8_t txData[4] = {0x11, 0x22, 0x33, 0x44};

/* 配置发送信息 */
flexcan_data_info_t txInfo;
txInfo.msg_id_type = FLEXCAN_MSG_ID_STD;   /* 标准帧 */
txInfo.data_length = 4;                     /* 4 字节数据 */
txInfo.is_remote   = false;                 /* 数据帧 */

/* ★ 阻塞发送 */
status_t ret = FLEXCAN_DRV_SendBlocking(
    0,              /* CAN0 */
    0,              /* 使用 MB0 */
    &txInfo,        /* 帧信息 */
    0x123,          /* CAN ID */
    txData,         /* 数据 */
    1000            /* 超时 1000ms */
);

if (ret == STATUS_SUCCESS) {
    /* 发送成功 */
} else if (ret == STATUS_TIMEOUT) {
    /* 发送超时——总线可能有问题 */
}
```

### 5.2 SDK 内部自动处理的

| SDK 内部操作 | 对应的硬件处理 |
|-------------|--------------|
| 检查 MB 是否空闲 | 读 CS CODE 不是 TX_ACTIVE |
| 写 ID 寄存器 | mb->ID = (id << 18)（标准帧）|
| 写数据到 WORD0/WORD1 | 小端序排列 |
| 写 CS 触发发送 | CODE=TX_ACTIVE(0x0C) |
| 等待发送完成 | 轮询 CODE=INACTIVE 或 IFLAG1 |
| 清除完成标志 | 写 1 清 IFLAG1 |

### 5.3 非阻塞（中断）发送

```c
/* 注册发送完成回调 */
void myCallback(uint8_t inst, flexcan_event_type_t ev,
                uint32_t buffIdx, flexcan_state_t *state)
{
    if (ev == FLEXCAN_EVENT_TX_COMPLETE) {
        /* MB buffIdx 发送完成 */
    }
}

void setup(void)
{
    FLEXCAN_DRV_InstallEventCallback(0, myCallback, NULL);
}

void send_nonblocking(void)
{
    /* 非阻塞发送——立即返回，完成后回调 */
    FLEXCAN_DRV_Send(0, 0, &txInfo, 0x123, txData);
}
```

---

## 第 6 篇：接收流程（SDK 方式） {#p6}

### 6.1 配置接收邮箱

```c
/* 配置 MB1 为接收邮箱（只需在初始化时配置一次） */
flexcan_data_info_t rxInfo;
rxInfo.msg_id_type = FLEXCAN_MSG_ID_STD;   /* 接收标准帧 */
rxInfo.data_length = 8;                     /* 最大接收 8 字节 */
rxInfo.is_remote   = false;

/* 配置 MB1 接收（第二个参数 0 表示不过滤 ID，接收所有） */
FLEXCAN_DRV_ConfigRxMb(0, 1, &rxInfo, 0);
```

### 6.2 阻塞接收

```c
flexcan_msgbuff_t rxMsgBuff;   /* 接收缓冲区 */

status_t ret = FLEXCAN_DRV_ReceiveBlocking(
    0,              /* CAN0 */
    1,              /* 使用 MB1 */
    &rxMsgBuff,     /* 接收缓冲区 */
    1000            /* 超时 1000ms */
);

if (ret == STATUS_SUCCESS) {
    /* 收到帧！从 rxMsgBuff 读取数据 */
    uint32_t rxId   = rxMsgBuff.msgId;         /* CAN ID */
    uint8_t  rxLen  = rxMsgBuff.dataLen;       /* 数据长度 */
    uint8_t *rxData = rxMsgBuff.data;          /* 数据指针 */

    printf("CAN: ID=0x%03X, DLC=%d, Data=", rxId, rxLen);
    for (uint8_t i = 0; i < rxLen; i++) {
        printf("%02X ", rxData[i]);
    }
    printf("\n");

    /* SDK 自动将 MB 重新设为 RX_EMPTY，继续接收 */
} else if (ret == STATUS_TIMEOUT) {
    /* 超时未收到帧 */
}
```

### 6.3 SDK 内部自动处理的

| SDK 内部操作 | 说明 |
|-------------|------|
| 轮询 IFLAG1 检查接收完成 | 检查是否有 MB 收到新帧 |
| 读取 CS CODE | 确认是 RX_FULL（0x0C）|
| 读取 ID 寄存器 | 自动解析标准/扩展帧 |
| 读取 WORD0/WORD1 | 小端序解析 |
| 清除 IFLAG1 | 写 1 清除标志位 |
| **重新设为 RX_EMPTY** | 这是最简单被忘的一步，SDK 自动处理 |

### 6.4 非阻塞（中断）接收

```c
/* 接收完成回调 */
void myCallback(uint8_t inst, flexcan_event_type_t ev,
                uint32_t buffIdx, flexcan_state_t *state)
{
    if (ev == FLEXCAN_EVENT_RX_COMPLETE) {
        /* MB buffIdx 收到新帧 */
        /* 在上层注册的 rxMsgBuff 中读取数据 */
    }
}

void setup(void)
{
    FLEXCAN_DRV_InstallEventCallback(0, myCallback, NULL);
    FLEXCAN_DRV_Receive(0, 1, &g_rxMsg);   /* 开始非阻塞接收 */
}
```

---

## 第 7 篇：SDK 快速参考 {#p7}

### 7.1 FlexCAN API 速查表

| API | 功能 | 阻塞 |
|-----|------|------|
| `FLEXCAN_DRV_Init()` | 初始化 FlexCAN | — |
| `FLEXCAN_DRV_Deinit()` | 关闭 FlexCAN | — |
| `FLEXCAN_DRV_GetDefaultConfig()` | 获取默认配置（500kbps） | — |
| `FLEXCAN_DRV_ConfigTxMb()` | 配置发送 MB | — |
| `FLEXCAN_DRV_ConfigRxMb()` | 配置接收 MB | — |
| `FLEXCAN_DRV_Send()` | 非阻塞发送 | 否 |
| `FLEXCAN_DRV_SendBlocking()` | 阻塞发送（带超时） | 是 |
| `FLEXCAN_DRV_Receive()` | 非阻塞接收 | 否 |
| `FLEXCAN_DRV_ReceiveBlocking()` | 阻塞接收（带超时） | 是 |
| `FLEXCAN_DRV_InstallEventCallback()` | 注册中断回调 | — |
| `FLEXCAN_DRV_GetTransferStatus()` | 查询传输状态 | — |

### 7.2 SDK 关键结构体

**`flexcan_user_config_t`**（初始化配置）：
```c
typedef struct {
    uint32_t max_num_mb;              /* MB 数量 */
    bool     is_rx_fifo_needed;       /* 是否使用 Rx FIFO */
    uint32_t flexcanMode;             /* 普通/监听/回环模式 */
    uint32_t bitrate;                 /* 波特率 (bps) */
    /* ... 其他字段 SDK 默认配置已覆盖 */
} flexcan_user_config_t;
```

**`flexcan_msgbuff_t`**（消息缓冲区数据）：
```c
typedef struct {
    uint32_t cs;           /* 状态码（内部使用） */
    uint32_t msgId;        /* CAN ID */
    uint8_t  data[64];     /* 数据 */
    uint8_t  dataLen;      /* 有效数据长度 */
} flexcan_msgbuff_t;
```

**`flexcan_data_info_t`**（收发配置）：
```c
typedef struct {
    uint32_t msg_id_type;  /* FLEXCAN_MSG_ID_STD / FLEXCAN_MSG_ID_EXT */
    uint32_t data_length;  /* 数据长度 */
    bool     is_remote;    /* 远程帧 */
} flexcan_data_info_t;
```

### 7.3 常用枚举值

```c
/* 工作模式 */
FLEXCAN_NORMAL_MODE       /* 普通模式 */
FLEXCAN_LISTEN_ONLY_MODE  /* 仅监听 */
FLEXCAN_LOOPBACK_MODE     /* 回环自收发测试 */

/* ID 类型 */
FLEXCAN_MSG_ID_STD        /* 标准帧 11 位 ID */
FLEXCAN_MSG_ID_EXT        /* 扩展帧 29 位 ID */

/* 回调事件 */
FLEXCAN_EVENT_RX_COMPLETE  /* 接收完成 */
FLEXCAN_EVENT_TX_COMPLETE  /* 发送完成 */
FLEXCAN_EVENT_ERROR        /* 错误发生 */
```

### 7.4 典型使用模式

**轮询模式**（适合学习和简单应用）：

```c
static flexcan_state_t   g_flexState;
static flexcan_msgbuff_t g_rxMsg;

void can_polling_demo(void)
{
    flexcan_user_config_t config;
    FLEXCAN_DRV_GetDefaultConfig(&config);
    config.max_num_mb = 3;
    FLEXCAN_DRV_Init(0, &g_flexState, &config);

    flexcan_data_info_t rxInfo;
    rxInfo.msg_id_type = FLEXCAN_MSG_ID_STD;
    rxInfo.data_length = 8;
    rxInfo.is_remote   = false;
    FLEXCAN_DRV_ConfigRxMb(0, 1, &rxInfo, 0);

    flexcan_data_info_t txInfo;
    txInfo.msg_id_type = FLEXCAN_MSG_ID_STD;
    txInfo.data_length = 4;
    txInfo.is_remote   = false;

    uint8_t txData[8] = {0x11, 0x22, 0x33, 0x44};

    while (1) {
        /* 周期性发送 */
        FLEXCAN_DRV_SendBlocking(0, 0, &txInfo, 0x100, txData, 100);

        /* 阻塞接收 500ms 超时 */
        if (FLEXCAN_DRV_ReceiveBlocking(0, 1, &g_rxMsg, 500)
            == STATUS_SUCCESS) {
            /* 处理接收 */
        }
    }
}
```

**中断模式**（适合生产应用）：

```c
void myCallback(uint8_t inst, flexcan_event_type_t ev,
                uint32_t buffIdx, flexcan_state_t *state)
{
    if (ev == FLEXCAN_EVENT_RX_COMPLETE) {
        /* 收到帧，数据在 g_rxMsg 中 */
    } else if (ev == FLEXCAN_EVENT_TX_COMPLETE) {
        /* 发送完成 */
    }
}

void can_irq_demo(void)
{
    flexcan_user_config_t config;
    FLEXCAN_DRV_GetDefaultConfig(&config);
    FLEXCAN_DRV_Init(0, &g_flexState, &config);
    FLEXCAN_DRV_InstallEventCallback(0, myCallback, NULL);

    FLEXCAN_DRV_ConfigRxMb(0, 1, &rxInfo, 0);
    FLEXCAN_DRV_Receive(0, 1, &g_rxMsg);   /* 开始非阻塞接收 */

    while (1) {
        /* 主循环做其他任务，CAN 回调异步处理 */
    }
}
```

### 7.5 常见坑位提醒

| # | 现象 | 原因 | SDK 如何帮助 |
|---|------|------|-------------|
| 1 | 初始化失败 | 忘记使能时钟 | SDK 自动处理 ✅ |
| 2 | 发送的 ID 错乱 | 写寄存器顺序不对 | SDK 内部顺序正确 ✅ |
| 3 | 收了一帧后不再收 | 忘记设回 RX_EMPTY | SDK 自动处理 ✅ |
| 4 | 发送超时 | 总线无终端电阻 | 硬件自检 |
| 5 | 收到的数据不对 | CRC 错误自动丢弃 | SDK 自动校验 |
| 6 | 中断不触发 | 忘记安装回调 | 按需安装 ✅ |

---

## 附录 A：波特率验算表（80MHz 时钟）

| 波特率 | PRESDIV | 总 TQ | 验算 |
|-------|---------|-------|------|
| 500k | 9 | 16 | 80MHz / (10 × 16) = 500k ✓ |
| 250k | 19 | 16 | 80MHz / (20 × 16) = 250k ✓ |
| 125k | 39 | 16 | 80MHz / (40 × 16) = 125k ✓ |

## 附录 B：FlexCAN SDK 初始化内部流程（参考）

| 概念步骤 | SDK API 对应 |
|---------|-------------|
| 使能 PCC 时钟 | `FLEXCAN_DRV_Init()` 内部自动处理 |
| 冻结 + 复位 | `FLEXCAN_DRV_Init()` 内部自动处理 |
| 配置 MCR | 根据 `config.max_num_mb` 自动计算 |
| 配置 CTRL1 | 根据 `config.bitrate` 自动计算 |
| 配置 MB | 清空后按需配置 |
| 退出冻结 | `FLEXCAN_DRV_Init()` 内部自动处理 |

---

**全文结束**

读完本文后，打开 `mcu/src/flexcan.c` 和 `mcu/include/flexcan.h`，
对照第 4~6 篇的 SDK API 示例，理解 `flexcan_init()`、`flexcan_send_msg()`、
`flexcan_recv_msg()` 三个函数如何封装 SDK API。

工程代码使用 SDK API 而非寄存器操作，SDK 头文件路径：
`mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/drivers/inc/flexcan_driver.h`
