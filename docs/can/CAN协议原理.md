# CAN 协议原理

> 基于 S32K144 FlexCAN 项目的 CAN 2.0B 协议基础知识，涵盖物理层、帧结构、位时序、
> 错误处理机制。理解这些概念是使用 `mcu/MCAL/Can` 驱动和排查 CAN 通信问题的前提。

---

## 1. 物理层：差分信号

### 1.1 为什么用差分信号

CAN 总线使用两根线传输数据：**CAN_H**（CAN High）和 **CAN_L**（CAN Low）。
接收方通过两根线的**电压差**判断逻辑值，而不是单根线的绝对电压。

```
        发送节点                          接收节点
    ┌──────────┐                     ┌──────────┐
    │ TX  ──┬──┤────── CAN_H ────────┤── R      │
    │       │  │                     │   │      │
    │       │  │                     │   ▼      │
    │       │  │                     │ 差分放大  │
    │       │  │                     │  器(比较) │
    │   ────┴──┤────── CAN_L ────────┤── R      │
    └──────────┘                     └──────────┘
```

差分信号的抗干扰原理：电磁噪声会同时耦合到 CAN_H 和 CAN_L 两根线上（共模干扰），
接收端的差分放大器只放大差值，因此共模噪声被大幅抑制。

### 1.2 显性与隐性电平

| 状态 | CAN_H 电压 | CAN_L 电压 | 差分电压 (Vdiff) | 逻辑值 | 驱动方式 |
|------|-----------|-----------|------------------|--------|---------|
| **显性** (Dominant) | ~3.5V | ~1.5V | ~2.0V | **0** | 收发器主动驱动 |
| **隐性** (Recessive) | ~2.5V | ~2.5V | ~0V | **1** | 终端电阻被动拉至 2.5V |

关键特性：**显性位"压倒"隐性位**。如果节点 A 发送隐性（1），节点 B 发送显性（0），
总线上实际表现的是显性（0）——这是 CAN 仲裁机制的基础。

```
    隐性(1):   CAN_H ──────── 2.5V
               CAN_L ──────── 2.5V
               差分 ──────── 0V

    显性(0):   CAN_H ────┐   3.5V
                        ├───
               CAN_L ────┘   1.5V
               差分 ──────── 2V
```

### 1.3 终端电阻（120Ω）

总线两端必须各接一个 **120Ω 电阻**，跨接在 CAN_H 和 CAN_L 之间。

```
      ECU-A                              ECU-B
   ┌────────┐                        ┌────────┐
   │        │   CAN_H ────────────────┤        │
   │       ├──┤~~~~~~~~~~~├──────────┤        │
   │  120Ω │                        │   120Ω │
   │       ├──┤~~~~~~~~~~~├──────────┤        │
   │        │   CAN_L ────────────────┤        │
   └────────┘                        └────────┘
```

终端电阻的两个作用：

1. **阻抗匹配**：防止信号在总线末端反射，反射会导致数据错乱
2. **隐性电平维持**：当没有节点驱动总线时，终端电阻将 CAN_H 和 CAN_L 拉到相同的
   2.5V 电平（隐性状态）

> 本项目的 S32K144 开发板上使用两个 62Ω 电阻串联（共 124Ω）作为终端电阻，
> 通过跳线帽选择是否接入。实测表明 120Ω 终端电阻是 CAN 通信的必要条件，
> 不接会导致发送持续失败。

---

## 2. CAN 帧结构

### 2.1 标准数据帧（11-bit ID）

一个完整的 CAN 2.0B 标准数据帧结构如下：

```
┌─────┬──────────┬─────┬─────┬──────┬──────┬──────────────┬──────┬──────┬──────┐
│ SOF │   ID     │ RTR │ IDE │  r0  │ DLC  │    DATA      │ CRC  │ ACK  │ EOF  │
│ 1bit│  11 bits │ 1bit│ 1bit│ 1bit │ 4bit │   0~8 bytes  │15bit │ 2bit │ 7bit │
└─────┴──────────┴─────┴─────┴──────┴──────┴──────────────┴──────┴──────┴──────┘
```

各字段说明：

| 字段 | 长度 | 说明 |
|------|------|------|
| **SOF** (Start of Frame) | 1 bit | 帧起始，固定为显性位（0）。所有节点用此刻对齐时钟 |
| **ID** (Identifier) | 11 bits | 帧标识符，决定仲裁优先级（越小优先级越高） |
| **RTR** (Remote Transmission Request) | 1 bit | 0 = 数据帧，1 = 远程帧（请求对端发数据） |
| **IDE** (Identifier Extension) | 1 bit | 0 = 标准帧（11-bit ID），1 = 扩展帧（29-bit ID） |
| **r0** (Reserved) | 1 bit | 保留位，发送 0 |
| **DLC** (Data Length Code) | 4 bits | 数据场字节数，取值范围 0~8 |
| **DATA** | 0~8 bytes | 有效载荷数据，小端序或大端序由上层协议约定 |
| **CRC** (Cyclic Redundancy Check) | 15 bits | 循环冗余校验，发送方计算、接收方验证 |
| **CRC Delimiter** | 1 bit | 固定隐性位，CRC 字段结束 |
| **ACK Slot** | 1 bit | 发送方发隐性位；接收方若 CRC 正确则回显性位覆盖 |
| **ACK Delimiter** | 1 bit | 固定隐性位 |
| **EOF** (End of Frame) | 7 bits | 帧结束，连续 7 个隐性位 |

### 2.2 扩展帧（29-bit ID）

扩展帧在标准帧基础上多了 18 位 ID（SRR + IDE + ID[17:0]）：

```
┌─────┬─────────┬─────┬─────┬────────────┬──────┬──────┬────┬──────────┐
│ SOF │ ID[28:18]│ SRR │ IDE │ ID[17:0]   │ r1/r0│ DLC  │DATA│ CRC~EOF  │
│ 1bit│  11 bits │ 1bit│ 1bit│  18 bits   │ 2bit │ 4bit │... │ (同标准帧)│
└─────┴─────────┴─────┴─────┴────────────┴──────┴──────┴────┴──────────┘
```

| | 标准帧 | 扩展帧 |
|--|--------|--------|
| ID 总位数 | 11 bits | 29 bits |
| ID 范围 | 0x000 ~ 0x7FF | 0x00000000 ~ 0x1FFFFFFF |
| 帧总长度（8 字节数据） | ~108 bits | ~128 bits |
| 常用场景 | 动力总成实时控制（引擎、ABS） | 诊断通信 (UDS/ISO 15765)、J1939 |
| IDE 位值 | 0 | 1 |

### 2.3 仲裁过程

多个节点同时尝试发送时，CAN 通过**逐位仲裁**决定谁获得总线使用权：

```
  节点A: ID=0x123 = 001 0010 0011
  节点B: ID=0x124 = 001 0010 0100
                          ↑
                     第 3 位：节点A 发 0（显性），节点B 发 0 → 都认为 OK
                     第 2 位：节点A 发 0，节点B 发 1 → 节点B 检测到总线是 0
                              但自己发了 1 → 仲裁失败 → 立即停止发送，转为接收

  结论：ID 小的节点（0x123）获得总线控制权
```

仲裁规则：
- 发送方同时**监听**总线电平
- 如果自己发送隐性（1）但总线上出现显性（0）→ 仲裁失败
- 仲裁失败的节点立即停止发送，转为接收模式
- **ID 越小，优先级越高**（因为更多前导零）

### 2.4 位填充 (Bit Stuffing)

**规则**：发送方在连续 5 个相同电平位之后，强制插入一个相反位。
接收方自动去除填充位。

```
原始数据:  000001111100000...
填充后:    00000[1]11111[0]00000[1]...
                    ↑      ↑       ↑
                  插入1   插入0   插入1
```

目的：
1. 提供足够的跳变沿供接收方同步时钟（CAN 没有独立时钟线）
2. 防止长串相同电平被误判为帧结束（EOF=7 个隐性位）
3. 填充位不计入 DLC，不参与 CRC 计算

---

## 3. 位时序 (Bit Timing)

### 3.1 时间量子 (Time Quantum, TQ)

CAN 将一个 bit 的持续时间进一步细分为若干个**时间量子 (TQ)**：

```
                    ←─────────── 一个 bit 的时间长度 ───────────→
                    ┌──────┬──────────┬───────────┬─────────────┐
                    │SYNC  │ PROP_SEG │PHASE_SEG1 │ PHASE_SEG2  │
                    │_SEG  │          │           │             │
                    │ 1 TQ │ 1~8 TQ   │  1~8 TQ   │  1~8 TQ     │
                    └──────┴──────────┴─────┬─────┴─────────────┘
                                            │
                                          采样点
                                    (在此刻读取总线电平)
```

| 时间段 | TQ 数 | 作用 |
|--------|-------|------|
| **SYNC_SEG** | 固定 1 TQ | 同步段：期望的跳变沿位置，所有节点在此对齐 |
| **PROP_SEG** | 1~8 TQ | 传播段：补偿总线物理传输延迟（信号在导线上传播的时间） |
| **PHASE_SEG1** | 1~8 TQ | 相位缓冲段 1：采样前的时间，可被同步跳转拉长 |
| **PHASE_SEG2** | 1~8 TQ | 相位缓冲段 2：采样后的时间，可被同步跳转缩短 |

SJW (Synchronization Jump Width) = 1~4 TQ：
当检测到跳变沿偏离 SYNC_SEG 时，允许 PHASE_SEG1 最多拉长或 PHASE_SEG2 最多缩短
SJW 个 TQ 来重新同步。SJW 限制了每次同步调整的最大步长。

### 3.2 采样点 (Sample Point)

接收方在 **PHASE_SEG1 末尾（即 PHASE_SEG2 开始前）** 读取总线电平。
此时刻的判决决定了"这一位是 0 还是 1"。

```
    采样点位置 = (1 + PROP_SEG + PHASE_SEG1) / 总 TQ 数 × 100%

    例：总 TQ=16, PROP=7, PSEG1=4
    采样点 = (1+7+4)/16 = 75%
```

一般推荐采样点在 **75%~87.5%**。采样点太早容易受振铃干扰，太晚则留给
PHASE_SEG2 的裕量不足（无法处理相位误差）。

### 3.3 波特率计算公式

```
                             F_can_clk
    波特率 (bps) = ───────────────────────────────
                    (预分频值 + 1) × 总 TQ 数

    总 TQ 数 = 1 (SYNC_SEG) + PROP_SEG + PHASE_SEG1 + PHASE_SEG2
```

**S32K144 计算实例**（PE 时钟来自 OSC，经 PLL 后送入 FlexCAN 外设）：

```
        项目验证配置 (CAN_CLK_SOURCE_OSC)：
        PROP_SEG=7, PHASE_SEG1=4, PHASE_SEG2=1, PRE_DIVIDER=0
        总 TQ = 1 + 7 + 4 + 1 = 13 TQ

        对应 DLC 为 8 的 CAN 帧，理论比特率：
        bitrate = F_can_pe / (0+1) / 13

        (注：CAN PAL 中 peClkSrc = CAN_CLK_SOURCE_OSC 选择 PE 时钟源；
              具体比特率取决于 SCG 时钟树配置下的 PE 时钟频率)
```

以下为典型 80MHz 外设时钟下的验算参考：

| 目标波特率 | PROP_SEG | PSEG1 | PSEG2 | 总 TQ | 预分频值 | 采样点 | 验算 |
|-----------|----------|-------|-------|-------|---------|--------|------|
| 500 kbps | 7 | 4 | 2 | 16 | 9 | 75.0% | 80M/(10×16)=500k |
| 250 kbps | 7 | 4 | 2 | 16 | 19 | 75.0% | 80M/(20×16)=250k |
| 125 kbps | 7 | 4 | 2 | 16 | 39 | 75.0% | 80M/(40×16)=125k |
| 1000 kbps | 3 | 3 | 2 | 9 | 7 | 77.8% | 80M/(8×9)=1111k |

### 3.4 项目中的位时序结构体

项目中位时序通过 `Can_ConfigType` 传递给 MCAL 驱动：

```c
// mcu/MCAL/Can/include/Can.h (第 60~82 行)
typedef struct {
    uint8_t controller;
    uint8_t max_num_mb;
    // ...
    uint8_t prop_seg;        // PROP_SEG (TQ 数)
    uint8_t phase_seg1;      // PHASE_SEG1 (TQ 数)
    uint8_t phase_seg2;      // PHASE_SEG2 (TQ 数)
    uint8_t pre_divider;     // 预分频值 (PRESDIV)
    uint8_t r_jumpwidth;     // SJW (同步跳转宽度)
    // ...
} Can_ConfigType;
```

Can.c 内部将这些字段映射到 NXP CAN PAL 的 `flexcan_time_segment_t`：

```c
// mcu/MCAL/Can/src/Can.c (第 52~57 行)
pal->nominalBitrate.propSeg    = mcal->prop_seg;
pal->nominalBitrate.phaseSeg1  = mcal->phase_seg1;
pal->nominalBitrate.phaseSeg2  = mcal->phase_seg2;
pal->nominalBitrate.preDivider = mcal->pre_divider;
pal->nominalBitrate.rJumpwidth = mcal->r_jumpwidth;
```

---

## 4. 错误管理

### 4.1 五种错误类型

| 错误类型 | 检测方式 | 示例 |
|---------|---------|------|
| **位错误** (Bit Error) | 发送方比较发出的位与总线实际电平 | 发了隐性(1)但总线是显性(0)（仲裁阶段除外） |
| **填充错误** (Stuff Error) | 连续 6 个相同电平位 | 接收方检测到 000000（应填充为 000001） |
| **CRC 错误** (CRC Error) | 接收方计算的 CRC 与帧内 CRC 不同 | 数据被干扰 |
| **格式错误** (Form Error) | 固定格式位（CRC/ACK 分隔符、EOF）出现错误电平 | 分隔符检测到显性位 |
| **ACK 错误** (ACK Error) | 发送方在 ACK Slot 检测不到显性位 | 总线上没有节点正确收到帧 |

### 4.2 错误计数器与三种状态

每个 CAN 节点内部维护两个错误计数器：

- **TEC** (Transmit Error Counter)：发送错误计数
- **REC** (Receive Error Counter)：接收错误计数

根据计数器值，节点处于以下三种状态之一：

```
     REC<128 且 TEC<128          REC≥128 或 TEC≥128           TEC≥256
    ┌──────────────┐          ┌──────────────────┐          ┌──────────────┐
    │              │  错误↑   │                  │  错误↑   │              │
    │ ERROR_ACTIVE │ ───────→ │  ERROR_PASSIVE   │ ───────→ │   BUS_OFF    │
    │  (正常通信)  │ ←─────── │ (可以通信但受限) │          │  (脱离总线)  │
    │              │  错误↓   │                  │          │              │
    └──────────────┘          └──────────────────┘          └──────────────┘
                                            │                       │
                                            │ 连续 128 次 11 个      │
                                            │ 隐性位后恢复            │ 需手动恢复
                                            ↓                       ↓
                                     ERROR_ACTIVE              ERROR_ACTIVE
```

| 状态 | TEC / REC | 行为 |
|------|-----------|------|
| **ERROR_ACTIVE** | 都 < 128 | 正常参与通信，发送 **Active Error Flag**（6 个显性位） |
| **ERROR_PASSIVE** | 任一 ≥ 128 | 仍可收发，但发送 **Passive Error Flag**（6 个隐性位），帧间隔更长 |
| **BUS_OFF** | TEC ≥ 256 | 逻辑上脱离总线，不再收发。须软件干预恢复 |

错误计数的增减规则：
- 成功发送/接收一帧 → 计数 -1（最小 0）
- 检测到错误 → +1（接收错误）或 +8（发送错误）
- 其他节点报错 → 自身 +8

### 4.3 在项目代码中的体现

当前 MCAL Can 驱动通过 CAN PAL 层间接获得错误状态，但项目中尚未实现
显式的 BUS_OFF 恢复逻辑（这是后续开发需补充的内容）。

```c
// mcu/MCAL/Can/src/Can.c (第 129~139 行)
// Can_SetControllerMode 当前仅维护模块状态变量，不执行硬件模式切换
// BUS_OFF 恢复需要通过该接口实现，当前为骨架状态
void Can_SetControllerMode(Can_ControllerType Controller,
                           Can_ControllerStateType Transition)
{
    if (Controller >= CAN_CONTROLLER_MAX || !Can_Initialized) return;
    if (Transition == CAN_CS_STARTED) {
        Can_State[Controller] = CAN_CS_STARTED;
    } else if (Transition == CAN_CS_STOPPED) {
        Can_State[Controller] = CAN_CS_STOPPED;
    }
}
```

---

## 5. CAN 与 CAN FD 的区别

本项目的 S32K144 支持 CAN FD (Flexible Data-rate)，但当前配置**仅使用经典 CAN 2.0B**：

| | CAN 2.0B | CAN FD |
|--|----------|--------|
| 最大速率 | 1 Mbps | 仲裁段 1 Mbps / 数据段最高 8 Mbps |
| 最大数据场 | 8 bytes | 64 bytes |
| 帧格式 | 单一速率 | 双速率（BRS 位切换） |
| 兼容性 | 所有 CAN 节点均可通信 | 需 CAN FD 控制器支持 |

```c
// mcu/MCAL/Can/src/Can.c (第 48~49 行)
// 明确禁用 CAN FD，使用经典 CAN
pal->enableFD    = false;
pal->payloadSize = CAN_PAYLOAD_SIZE_8;   // 最大 8 字节
```

---

## 6. 关键要点总结

| 要点 | 一句话 |
|------|--------|
| 差分信号 | CAN_H - CAN_L = 2V (显性/0)，0V (隐性/1)，抗共模干扰 |
| 仲裁 | ID 越小优先级越高，显性位压倒隐性位 |
| 位填充 | 连续 5 个相同位后插入相反位，提供跳变沿 |
| 位时序 | 一个 bit 分多段 TQ，采样点建议 75%~87.5% |
| 终端电阻 | 总线两端各 120Ω，不接则信号反射导致通信失败 |
| BUS_OFF | TEC≥256 时脱离总线，需手动恢复 |
| 经典 vs FD | 本项目使用 CAN 2.0B，enableFD=false |

---

## 参考文件

| 文件 | 内容 |
|------|------|
| `mcu/MCAL/Can/include/Can.h` | MCAL Can 驱动头文件，包含 Can_ConfigType 位时序字段定义 |
| `mcu/MCAL/Can/src/Can.c` | MCAL Can 驱动实现，Can_BuildPalConfig() 完成位时序映射 |
| `docs/从零学CAN.md` | 项目配套教程，补充 SDK 使用方式和寄存器操作基础 |
