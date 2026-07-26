# Can_SignalDefType 与 AUTOSAR COM 标准对比

> **定位**：过渡期参考文档。随 AUTOSAR 各层逐步实现，本文档中的计划内容会逐步落地为独立文档（如 [2_MCAL_Can驱动详解](./2_MCAL_Can驱动详解.md)、[3_CanIf_CAN接口层详解](./3_CanIf_CAN接口层详解.md) 等）。全部实现后本文档可归档。

> 对比当前自定义 `Can_SignalDefType` 与 AUTOSAR SWC→RTE→Com→PduR→CanTp→CanIf→Can 六层模型，
> 给出从"扁平 DBC 等价物"到"分层 AUTOSAR 架构"的迁移路线。
>
> **当前状态**: Can→CanIf→CanTp→PduR 四层已调通（中断驱动 RX + FC 流控 + 最小路由），
> Com + RTE 为骨架，本文档的阶段 1/2 已实现，阶段 3 待推进。

---

## 1. 从 Can_SignalDefType 说起 — 当前方案做了什么

### 1.1 当前的数据流

```
CAN 总线
   │
   ▼
 收到 CAN 帧: ID = 0x123, data[8] = {0x11, 0x22, 0x33, ...}
   │
   ▼
 查 Can_SignalMap[]:
   Can_SignalDefType {can_id=0x123, start_bit=0, len=8, is_big_endian=0,
                       scale_num=1, scale_den=1, offset=0, target_channel=1}
   │
   ├── raw 值 = data[0] = 0x11 = 17
   ├── 物理值 = 17 * 1/1 + 0 = 17
   └── target_channel=1 → 从 SPI1 发给 SoC
```

### 1.2 Can_SignalDefType 结构体回顾

来自 [Can_Cfg.h](../../mcu/MCAL/Can/config/Can_Cfg.h) 第 47~56 行：

```c
typedef struct {
    uint32_t can_id;          // 属于哪条 CAN 帧
    uint8_t  start_bit;       // 在帧内从哪个 bit 开始
    uint8_t  length;          // 占多少 bit
    uint8_t  is_big_endian;   // Intel(LSB) 还是 Motorola(MSB)
    uint16_t scale_num;       // 物理值 = raw * scale_num / scale_den + offset
    uint16_t scale_den;
    int16_t  offset;
    uint8_t  target_channel;  // 转发到哪个 SPI 通道
} Can_SignalDefType;
```

### 1.3 这个方案的问题

```
┌─────────────────────────────────────────────────────┐
│              Can_SignalDefType                        │
│                                                       │
│  ┌──────────┐  ┌──────────┐  ┌────────────────────┐  │
│  │ CAN 层信息 │  │ 信号层信息 │  │     路由层信息      │  │
│  │ can_id    │  │ start_bit│  │ target_channel     │  │
│  │           │  │ length   │  │                    │  │
│  │           │  │ endian   │  │                    │  │
│  │           │  │ scale    │  │                    │  │
│  │           │  │ offset   │  │                    │  │
│  └──────────┘  └──────────┘  └────────────────────┘  │
│        三层职责全部揉在一个扁平 struct 里               │
└─────────────────────────────────────────────────────┘
```

| 问题 | 后果 |
|------|------|
| 三层职责（CAN ID / 信号定义 / 路由）全部耦合 | 改一个信号的路由就要改整个结构体 |
| 没有"信号名"概念 | 只能靠索引访问，不可读 |
| 没有 I-PDU 概念 | 无法做信号的批量打包/解包 |
| 信号数量增长时不可维护 | 所有信号平铺在一个大数组里 |
| 与 AUTOSAR 标准不兼容 | 不能跟 Com/PduR/CanIf 对接 |

---

## 2. AUTOSAR COM 标准怎么做 — 分层模型

### 2.1 核心思想：信号 ≠ CAN 帧

AUTOSAR COM 的核心洞察是：**应用层关心的是"车速"这个信号，不关心它装在哪个 CAN ID 的第几个 bit**。

所以 AUTOSAR 把这个映射拆成了**六层**，每一层只做一件事：

```
┌───────────────────────────────────────────────────────────┐
│  应用层 (SWC)                                              │
│  speed = Rte_Read_VehicleSpeed();                          │
│  if (speed > 100) brake();  // SWC 用强类型物理值写逻辑     │
│  SWC 只看到命名物理值 + 类型，不知道 CAN、不知道 bit 位置     │
├───────────────────────────────────────────────────────────┤
│  运行时环境 (RTE)           ┌─────────────────────────┐    │
│  Rte_Read_VehicleSpeed()    │ RTE 的职责:              │    │
│  → 调 Com_ReceiveSignal()   │ · SWC端口 ↔ Com信号映射  │    │
│  → raw * factor + offset    │ · 物理值转换(scale/offset)│    │
│  → 返回 uint16_t km/h       │ · 强类型包装             │    │
│                              └─────────────────────────┘    │
├───────────────────────────────────────────────────────────┤
│  信号层 (Com)               ┌─────────────────────────┐    │
│  Com_ReceiveSignal(         │ Com_SignalConfigType    │    │
│    COM_SIGNAL_ID_XXX)       │ .signal_id = 0x0001    │    │
│  → 从 I-PDU 中提取 bit 0~7  │ .ipdu_id   = 3         │    │
│  → 返回 raw = 0x0011        │ .bit_position = 0      │    │
│  Com 只管 bit 拆装，不管物理值│ .bit_size    = 8       │    │
│                              │ .byte_order  = Intel   │    │
│                              └─────────────────────────┘    │
├───────────────────────────────────────────────────────────┤
│  PDU 层 (Com)               ┌─────────────────────────┐    │
│  Com_IPduGroupStart(3)      │ Com_IPduConfigType      │    │
│  → 周期发送或事件发送         │ .ipdu_id    = 3         │    │
│                              │ .can_id     = 0x123     │    │
│                              │ .dlc        = 8         │    │
│                              │ .cycle_time = 100ms     │    │
│                              └─────────────────────────┘    │
├───────────────────────────────────────────────────────────┤
│  传输层 (CanTp)             ┌─────────────────────────┐    │
│  CanTp_Transmit(N-PDU)      │ CanTp_TxConfigType       │    │
│  → SF/FF/MF 分段发送         │ .n_pdu_id    = 3        │    │
│  → FC 流控帧处理             │ .can_id      = 0x123    │    │
│  → 接收方重组                │ .st_min      = 0        │    │
│                              │ .block_size  = 8        │    │
│                              └─────────────────────────┘    │
├───────────────────────────────────────────────────────────┤
│  接口层 (CanIf)             ┌─────────────────────────┐    │
│  CanIf_Transmit(PDU_3)      │ CanIf_PduConfigType     │    │
│  → PDU 映射到 CAN 控制器/ID  │ .pdu_id        = 3     │    │
│                              │ .controller_id = 0     │    │
│                              │ .can_id        = 0x123 │    │
│                              └─────────────────────────┘    │
├───────────────────────────────────────────────────────────┤
│  硬件层 (Can)                                             │
│  Can_Write(TX_MB, PDU) → 物理 CAN 帧发出                   │
└───────────────────────────────────────────────────────────┘
```

> **关键**：SWC 永远不直接调 `Com_ReceiveSignal()`，RTE 永远是中间人。

### 2.2 各层类型的职责单一化

| 层 | 类型 | 职责 | 只关心 |
|----|------|------|--------|
| RTE (运行时环境) | —（代码生成） | SWC 端口 ↔ Com 信号映射，物理值转换，强类型包装 | `port_name`, `signal_id`, `scale`, `offset`, `unit` |
| Com (信号) | `Com_SignalConfigType` | 信号 ↔ I-PDU 的 bit 级拆装 | `signal_id`, `bit_position`, `bit_size` |
| Com (PDU) | `Com_IPduConfigType` | I-PDU ↔ CAN ID 的映射 | `ipdu_id`, `can_id`, `dlc`, `cycle_time` |
| PduR (路由) | `PduR_RouteConfigType` | PDU 在模块间路由 | `src_pdu_id`, `dst_pdu_id`, `src_module`, `dst_module` |
| CanTp (传输) | `CanTp_TxConfigType` | N-PDU 分段/重组 + FC 流控 | `n_pdu_id`, `can_id`, `st_min`, `block_size` |
| CanIf (接口) | `CanIf_PduConfigType` | PDU ↔ 硬件实例映射 | `pdu_id`, `controller_id`, `can_id`, `dlc` |

> **RTE 是 SWC 和 Com 之间的胶水层**。SWC 调 `Rte_Read_VehicleSpeed()`，RTE 内部调 `Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED, &raw)`，再做 `raw * factor + offset` 得到物理值，返回给 SWC。SWC 永远不直接碰 Com API。

**关键差异**：AUTOSAR 把 `Can_SignalDefType` 的 9 个字段拆分到了 6 个独立的层级中，每层只关注自己的抽象层级。

---

## 3. Can_SignalDefType vs AUTOSAR COM 逐字段对比

把 `Can_SignalDefType` 的 9 个字段拆解，看每个字段在 AUTOSAR 标准中应该放在哪：

### 3.1 字段映射表

| Can_SignalDefType 字段 | 属于 AUTOSAR 哪层 | 对应类型和字段 | 是否已有 | 说明 |
|---|---|---|---|---|
| `can_id` | Com + CanIf | `Com_IPduConfigType.can_id` / `CanIf_PduConfigType.can_id` | **已有** ✅ | 两个地方都要存：Com 层需要知道 PDU 对应哪个 CAN ID 以便打包；CanIf 层需要知道 PDU 发给哪个控制器和 ID |
| `start_bit` | Com | `Com_SignalConfigType.bit_position` | **已有** ✅ | 信号在 I-PDU 载荷中的起始位 |
| `length` | Com | `Com_SignalConfigType.bit_size` | **已有** ✅ | 信号的位宽 |
| `is_big_endian` | Com | `Com_SignalConfigType` — 需新增字段 | **缺失** ⚠️ | 现有的 `Com_SignalConfigType` 没有字节序字段，只有 `is_signed` |
| `scale_num` | RTE | — | **缺失** ⚠️ | RTE 负责物理值转换：`physical = raw * scale_num / scale_den + offset`。Com 层只返回 raw 值，不管物理意义 |
| `scale_den` | RTE | — | **缺失** ⚠️ | 同上——Com 是信号拆装层，物理换算属于 RTE 的职责 |
| `offset` | RTE | — | **缺失** ⚠️ | 同上 |
| `target_channel` | PduR | `PduR_RouteConfigType`（`src_module` / `dst_module`） | **已有** ✅ | PduR 的路由表就是做这个的：把 PDU 从 CanTp/CanIf 路由到 SpiIf 或其他模块 |
| （信号名称） | COM_SIGNAL_ID | `Com_SignalConfigType.signal_id` + 宏/枚举 | **已有** ✅ | 通过 `Com_SignalIdType` + signal_id 实现信号命名 |

### 3.2 现有骨架中需要补充的字段

```c
// Com_SignalConfigType（当前）          → 需要补充的
typedef struct {
    uint16_t signal_id;      // ✅ 已有
    uint16_t ipdu_id;        // ✅ 已有
    uint8_t  bit_position;   // ✅ 已有
    uint8_t  bit_size;       // ✅ 已有
    uint8_t  is_signed;      // ✅ 已有
    // --- 以下需新增 ---
    uint8_t  is_big_endian;  // ⚠️ 缺失：Intel(LSB first) / Motorola(MSB first)
} Com_SignalConfigType;

// 物理值转换不放在 Com 层，放在 RTE 层：
// RTE 从 YAML/DBC 读取 scale/offset，生成每个信号的 Rte_Read_xxx() 函数，
// 函数内部调 Com_ReceiveSignal() 拿 raw 值，再做物理转换
```

---

## 4. 改造路线：三步逐步对齐 AUTOSAR

### 4.1 阶段 1：YAML 信号矩阵 + 代码自动生成（最小改动）

**不改变任何运行时代码**，只是把信号定义从手写 C 数组改为 YAML + 生成脚本。

```
signals.yaml（单一数据源）
      │
      ├──→ generate_mcu_config.py → Can_SignalMap[] C 数组
      │                              （保持 Can_SignalDefType 不变）
      │
      └──→ SoC ConfigManager 加载 YAML
            （只取 name / unit / scale / offset，忽略 can_id / start_bit）
```

YAML 格式设计：

```yaml
# signals.yaml — 信号矩阵定义（DBC 等效）
# 一源多目标: MCU 生成 C 数组, SoC 加载物理描述

signals:
  - name: VehicleSpeed
    can_id: 0x123
    start_bit: 0
    length: 8
    byte_order: intel          # intel = LSB first, motorola = MSB first
    scale:
      num: 1
      den: 1
    offset: 0
    unit: km/h
    cycle_time_ms: 100         # 仅 MCU 侧需要（周期打包）

  - name: EngineRPM
    can_id: 0x123
    start_bit: 8
    length: 16
    byte_order: motorola
    scale:
      num: 1
      den: 4                   # 物理值 = raw * 0.25
    offset: 0
    unit: rpm
    cycle_time_ms: 100

  - name: CoolantTemp
    can_id: 0x456
    start_bit: 0
    length: 8
    byte_order: intel
    scale:
      num: 1
      den: 1
    offset: -40                # 物理值 = raw - 40
    unit: degC
    cycle_time_ms: 500
```

MCU 侧生成的 C 代码不变（因为阶段 1 不改变运行时）：

```c
// 由 generate_mcu_config.py 自动生成 — 不要手改
const Can_SignalDefType Can_SignalMap[] = {
    {
        .can_id         = 0x123,
        .start_bit      = 0,
        .length         = 8,
        .is_big_endian  = 0,   // intel
        .scale_num      = 1,
        .scale_den      = 1,
        .offset         = 0,
        .target_channel = 1,   // 默认全到通道 1，后续 PduR 接管
    },
    {
        .can_id         = 0x123,
        .start_bit      = 8,
        .length         = 16,
        .is_big_endian  = 1,   // motorola
        .scale_num      = 1,
        .scale_den      = 4,
        .offset         = 0,
        .target_channel = 1,
    },
    // ...
};
const uint8_t Can_SignalMap_Count = 2;
```

SoC 侧加载同一份 YAML，但**只关心物理层信息**（因为 MCU 的 RTE 层已经完成了 raw→physical 转换）：

```cpp
// SoC 加载后：
// can_id / start_bit / byte_order — 直接丢弃，SoC 不需要
// name / unit / scale / offset   — 用来生成 CanSignalDef（纯物理描述）
```

### 4.2 阶段 2：启用 Com + CanIf — 废弃 Can_SignalDefType

**这是核心改造**。YAML 不再生成 `Can_SignalDefType`，而是生成 AUTOSAR 格式的配置数组。

```
signals.yaml
      │
      ▼
generate_autosar_com_cfg.py
      │
      ├──→ Com_Cfg.c:  Com_SignalConfig[] + Com_IPduConfig[]
      ├──→ CanIf_Cfg.c: CanIf_PduConfig[]
      └──→ Com_SignalId.h: #define COM_SIGNAL_ID_VEHICLE_SPEED  0x0001
                            #define COM_SIGNAL_ID_ENGINE_RPM    0x0002
```

运行时数据流：

```
CAN 帧到达 (ID=0x123)
  │
  ▼
Can_Read() → Can_PduType {id=0x123, data[]}
  │
  ▼
CanIf_RxIndication() → PduId 翻译
  │
  ▼
CanTp_RxIndication() → SF/FF/MF 重组 (如需要)
  │
  ▼
PduR_CanTpRxIndication() → 路由到 Com
  │
  ▼
Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED)
  │ 查 Com_SignalConfig[0]: ipdu_id=3, bit_position=0, bit_size=8
  │ 从 I-PDU 3 的 data[] 中提取 bit 0~7 → raw=0x11
  │ Com 只返回 raw 值，不做物理转换
  ▼
Rte_Read_VehicleSpeed()          ← RTE 层
  │ 调 Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED, &raw)
  │ 查信号表: scale=1/1, offset=0 → physical = 17
  │ 返回 Rte_VehicleSpeedType (uint16_t, 单位 km/h)
  ▼
SWC: speed = Rte_Read_VehicleSpeed();
     if (speed > 100) { brake(); }
```

关键区别：

| | 阶段 1 (Can_SignalDefType) | 阶段 2 (AUTOSAR Com + RTE) |
|---|---|---|
| 信号命名 | ❌ 靠数组索引 | ✅ `COM_SIGNAL_ID_VEHICLE_SPEED` 宏 |
| 信号拆装 | 手写代码循环查表 | `Com_ReceiveSignal()` 标准 API |
| I-PDU 概念 | ❌ 无 | ✅ 信号打包成 I-PDU → 统一收发 |
| 物理转换 | 耦合在 struct 里 | RTE 层 `Rte_Read_xxx()` 函数负责 |
| SWC 接口 | 无类型，裸 bytes | ✅ 强类型 `Rte_VehicleSpeedType` |
| 配置来源 | 手写 C 数组 | YAML → Python 自动生成 |

### 4.3 阶段 3：PduR 路由层完整启用

PduR 接管所有模块间路由，不再需要 `target_channel` 硬编码。

```
signals.yaml 中新增 route 字段:
  - name: VehicleSpeed
    ...
    route:
      src: can
      dst: spi                  # ← 替换原来的 target_channel=1

生成 PduR_RouteConfig[]:
  {.src_pdu_id=3, .dst_pdu_id=3, .src_module=MOD_CANIF, .dst_module=MOD_SPIIF}
```

数据流变成完全标准化的 AUTOSAR 六层路径：

```
CAN 帧
  → Can_Read()                                   ← MCAL 硬件层
  → CanIf_RxIndication()                         ← ECU 抽象层
  → CanTp_RxIndication()   SF/FF/MF 分段/重组     ← 传输层
  → PduR_CanTpRxIndication()  查 PduR_RouteConfig → dst_module=SPIIF  ← 路由层
  → Com_ReceiveSignal()       从 I-PDU 拆出 raw 值                     ← 信号层
  → Rte_Read_VehicleSpeed()   raw → physical (scale/offset)            ← RTE 层
  → SpiIf 发送给 SoC
  → SoC 收到: "VehicleSpeed = 17 km/h" （物理值，不含任何 CAN 信息）
```

### 4.4 三阶段汇总

```
阶段 1（✅ 已实现）                阶段 2（✅ 已实现）              阶段 3（⏳ 待推进）
─────────────────                  ────────────                   ────────────
signals.yaml                       signals.yaml                   signals.yaml
  │                                  │                              │
  ▼                                  ▼                              ▼
Can_SignalDefType[]                Com_SignalConfig[]             Com_SignalConfig[]
(扁平数组, 不改代码)               Com_IPduConfig[]               Com_IPduConfig[]
                                   CanIf_PduConfig[]              CanIf_PduConfig[]
                                   CanTp_TxConfig[]               CanTp_TxConfig[]
                                   PduR_RouteConfig[]             PduR_RouteConfig[]
                                   Com_SignalId.h                 Com_SignalId.h
                                   Rte_SignalAccess.c              Rte_SignalAccess.c
                                   Rte_Type.h                      Rte_Type.h

Can_Read() + 手写循环查表           Can → CanIf → CanTp → PduR     同阶段2 + Com信号拆装
                                   (四层链路已验证)                 + RTE 物理值转换
                                                                    + SWC 强类型接口
```

---

## 5. DBC 模拟方案：YAML 信号矩阵

### 5.1 为什么不用真正的 DBC？

| | DBC 文件 | YAML |
|--|---------|------|
| 解析库 | 需要 dbc-parser / cantools (Python) | 标准 YAML 库（任何语言都有） |
| 可读性 | 专有格式，非汽车行业看不懂 | 通用格式，文本即可阅读 |
| 字段扩展 | 受 DBC 规范限制 | 自由扩展（加 unit、route 等自定义字段） |
| 工具链依赖 | Vector 工具链 | 零依赖 |
| 本项目适配 | 过重 | 刚好 |

**本项目的 YAML 格式不是 DBC 格式的替代品，而是 DBC 概念的精简模拟**——只取信号拆装需要的字段，不引入完整的 DBC 解析栈。

### 5.2 一源多目标的设计原则

```
                        ┌──────────────────────┐
                        │    signals.yaml       │
                        │    (单一数据源)         │
                        └──────┬───────────────┘
                               │
               ┌───────────────┼───────────────┐
               │               │               │
               ▼               ▼               ▼
         ┌──────────┐   ┌──────────┐   ┌──────────┐
         │ MCU 侧    │   │ SoC 侧    │   │ 文档      │
         │ C配置生成  │   │ 物理描述   │   │ 自动生成  │
         └──────────┘   └──────────┘   └──────────┘
               │               │               │
               ▼               ▼               ▼
    全部字段:          name/unit/         可读的信号清单
    can_id/start_bit/  scale/offset
    length/byte_order  （丢弃 CAN 字段）
    scale/offset/cycle

    生成:
    Com_SignalConfig[]   加载为:
    Com_IPduConfig[]     CanSignalDef[]
    CanIf_PduConfig[]    （仅物理层）
    Rte_SignalAccess.c   ← RTE 层：Rte_Read_xxx() 函数（含物理转换）
    Rte_Type.h           ← RTE 层：强类型定义
```

**核心原则**：
- YAML 包含**完整的**信号描述（CAN 层 + 物理层）
- MCU 侧代码生成器**使用全部字段**：
  - CAN 层字段 → 生成 `Com_SignalConfig[]` + `Com_IPduConfig[]` + `CanIf_PduConfig[]`（Com/CanIf 配置表）
  - 物理层字段 → 生成 `Rte_Read_xxx()` / `Rte_Write_xxx()` 函数（含 scale/offset 转换）
- SoC 侧加载器**只取 name/unit/scale/offset**——因为 SoC 收到的已经是 MCU RTE 层解析好的物理值
- SoC 永远不需要知道 `can_id`、`start_bit`、`byte_order`

### 5.3 完整的 YAML 字段定义

```yaml
# 每个信号的所有字段
signals:
  - name: string             # 信号名称（如 "VehicleSpeed"）
    can_id: hex              # CAN 报文 ID（仅 MCU 需要）
    start_bit: int           # 起始位 (0~63)
    length: int              # 位宽 (1~32)
    byte_order: intel|motorola  # 字节序
    is_signed: bool          # 是否有符号
    scale:                   # 物理值 = raw * scale.num/scale.den + offset
      num: int
      den: int               # 分母=1 表示 num/1
    offset: int              # 偏移量
    unit: string             # 单位（RTE 类型定义 + SoC / 文档需要）
    cycle_time_ms: int       # 发送周期（仅 MCU TX 需要，0=事件触发）
    route:                   # 路由目标（阶段 3 启用）
      src: can|spi|lin
      dst: can|spi|lin
```

### 5.4 代码生成脚本思路

```python
# generate_autosar_com_cfg.py
# 从 signals.yaml 生成 MCU 侧的所有 C 配置文件

import yaml

def generate(signals_yaml_path, output_dir):
    with open(signals_yaml_path) as f:
        data = yaml.safe_load(f)

    # 1. 按 can_id 分组 → 生成 Com_IPduConfig[]
    ipdus = {}
    signal_id = 1
    ipdu_id = 1
    for sig in data['signals']:
        if sig['can_id'] not in ipdus:
            ipdus[sig['can_id']] = {
                'ipdu_id': ipdu_id,
                'can_id': sig['can_id'],
                'dlc': 8,  # 默认 8 字节
                'cycle_time': sig.get('cycle_time_ms', 0),
                'signals': []
            }
            ipdu_id += 1
        ipdus[sig['can_id']]['signals'].append({
            'signal_id': signal_id,
            'signal_name': sig['name'],
            'bit_position': sig['start_bit'],
            'bit_size': sig['length'],
            'is_signed': sig.get('is_signed', False),
            'is_big_endian': 1 if sig['byte_order'] == 'motorola' else 0,
            'scale_num': sig['scale']['num'],
            'scale_den': sig['scale']['den'],
            'offset': sig['offset'],
        })
        signal_id += 1

    # 2. 生成 Com_Cfg.c, CanIf_Cfg.c, Com_SignalId.h
    write_com_signal_config(ipdus, output_dir)
    write_com_ipdu_config(ipdus, output_dir)
    write_canif_pdu_config(ipdus, output_dir)
    write_signal_id_macros(ipdus, output_dir)

    # 3. 生成 RTE 层信号访问函数 (Rte_SignalAccess.c + Rte_Type.h)
    #    每个信号生成一个 Rte_Read_xxx() 函数：
    #      Rte_VehicleSpeedType Rte_Read_VehicleSpeed(void) {
    #          uint16_t raw;
    #          Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED, &raw);
    #          return (Rte_VehicleSpeedType)(raw * 1 / 1 + 0);
    #      }
    write_rte_signal_access(ipdus, output_dir)
    write_rte_types(ipdus, output_dir)
```

---

## 6. 改造前后数据流对比

### 6.1 改造前（当前 Can_SignalDefType）

```
  CAN 帧到达 (ID=0x123)
       │
       ▼
  main.c: Can_Read(0, RX_MB, &rx_msg)
       │
       ▼
  手写 for 循环遍历 Can_SignalMap[]
       │
       ├── 匹配 can_id == 0x123?
       │   ├── 是 → 按 start_bit/length/endian 提取 raw
       │   │       按 scale/offset 算物理值
       │   │       按 target_channel 选 SPI 口 → 发给 SoC
       │   └── 否 → 继续下一个
       │
       ▼
  SoC 收到: data[] = {0x11, ...}  ← 还是 raw bytes! 没有信号名!
```

**问题**：SoC 收到的仍然是原始数据流，需要自己在 C++ 端再做一遍 `CanSignalDef` 解析——等于两边都做相同的事。

### 6.2 改造后（完整 AUTOSAR 六层：Can → CanIf → CanTp → PduR → Com → RTE → SWC）

```
  CAN 帧到达 (ID=0x123)
       │
       ▼
  ┌─ Can (MCAL 硬件层) ─────────────────────────────────────┐
  │ Can_Read() → Can_PduType {id=0x123, data[8]}            │
  └─────────────────────────────────────────────────────────┘
       │
       ▼
  ┌─ CanIf (ECU 抽象层) ────────────────────────────────────┐
  │ CanIf_RxIndication() → 查 CanIf_PduConfig[]             │
  │ → PduId 翻译 → 转发给 CanTp                             │
  └─────────────────────────────────────────────────────────┘
       │
       ▼
  ┌─ CanTp (传输层) ────────────────────────────────────────┐
  │ CanTp_RxIndication():                                   │
  │   → 解析 N-PCI: SF(单帧)/FF(首帧)/CF(连续帧)              │
  │   → FF: 发送 FC 流控帧 (BS+STmin)                        │
  │   → 多帧重组为完整 N-PDU                                  │
  │   → 调 PduR_CanTpRxIndication()                         │
  └─────────────────────────────────────────────────────────┘
       │
       ▼
  ┌─ PduR (路由层) ─────────────────────────────────────────┐
  │ PduR_CanTpRxIndication() → 查 PduR_RouteConfig[]       │
  │ → src=CanTp, dst=Com → 调 Com_RxIndication()            │
  └─────────────────────────────────────────────────────────┘
       │
       ▼
  ┌─ Com (信号拆装层) ──────────────────────────────────────┐
  │ Com_MainFunctionRx():                                   │
  │   Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED)        │
  │   → 查 Com_SignalConfig[sig_id]                         │
  │   → ipdu_id=3, bit_position=0, bit_size=8               │
  │   → 从 I-PDU data[] bit 0~7 提取 raw = 0x0011           │
  │   Com_ReceiveSignal(COM_SIGNAL_ID_ENGINE_RPM)            │
  │   → raw = 0x3200                                        │
  │   Com_ReceiveSignal(COM_SIGNAL_ID_COOLANT_TEMP)          │
  │   → raw = 0x7D                                          │
  │                                                          │
  │   Com 只返回 raw 值，不做物理转换！                        │
  └─────────────────────────────────────────────────────────┘
       │
       ▼
  ┌─ RTE (运行时环境) ──────────────────────────────────────┐
  │ Rte_Read_VehicleSpeed():                                │
  │   1. Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED, &raw)│
  │   2. physical = raw * 1/1 + 0 = 17                      │
  │   3. return (Rte_VehicleSpeedType)17;  // uint16_t km/h │
  │                                                          │
  │ Rte_Read_EngineRPM():                                    │
  │   1. Com_ReceiveSignal(COM_SIGNAL_ID_ENGINE_RPM, &raw)   │
  │   2. physical = raw * 256/1024 + 0 = 3200               │
  │   3. return (Rte_EngineRpmType)3200;  // uint16_t rpm   │
  │                                                          │
  │ Rte_Read_CoolantTemp():                                  │
  │   1. Com_ReceiveSignal(COM_SIGNAL_ID_COOLANT_TEMP, &raw) │
  │   2. physical = raw * 1/1 + (-40) = 85                  │
  │   3. return (Rte_CoolantTempType)85;  // uint8_t degC   │
  └─────────────────────────────────────────────────────────┘
       │
       ▼
  ┌─ SWC (应用层) ──────────────────────────────────────────┐
  │ speed = Rte_Read_VehicleSpeed();   // 17 km/h           │
  │ rpm   = Rte_Read_EngineRPM();      // 3200 rpm          │
  │ temp  = Rte_Read_CoolantTemp();    // 85 degC           │
  │                                                          │
  │ if (temp > 90) { fan_on(); }  ← 强类型物理值写业务逻辑   │
  └─────────────────────────────────────────────────────────┘
       │
       ▼ (PduR 路由到 SpiIf)
  SoC 收到（JSON 格式）:
    {"VehicleSpeed": {"value": 17, "unit": "km/h"},
     "EngineRPM":    {"value": 3200, "unit": "rpm"},
     "CoolantTemp":  {"value": 85, "unit": "degC"}}
    ← 只有命名物理值，零 CAN 信息!
```

**改进**：
- SoC 不再需要知道 CAN ID、start_bit、byte_order
- CanTp 层处理 >8 字节的大帧分段和流控，上层无感知
- PduR 层自动路由，新增接收模块无需改代码
- RTE 层完成 raw → physical 转换（scale/offset），SWC 拿到的是可直接使用的物理值
- Com 层只管 bit 级拆装，职责单一
- 同一个 YAML 的物理层字段（name/unit/scale/offset）给 RTE 代码生成用
- MCU 和 SoC 的解析逻辑不重复

---

## 7. 涉及的文件清单

### 7.1 新增文件

| 文件 | 内容 |
|------|------|
| `mcu/Config/signals.yaml` | 信号矩阵定义（单一数据源） |
| `mcu/Tools/generate_autosar_com_cfg.py` | 从 YAML 生成 C 配置数组 + RTE 函数 |
| `mcu/Services/Com/config/Com_Cfg.c` | 自动生成的信号/IPDU 配置 |
| `mcu/Services/Com/config/Com_SignalId.h` | 自动生成的信号 ID 宏 |
| `mcu/Services/CanTp/config/CanTp_Cfg.c` | 自动生成的 CanTp 传输层配置（SF/FF/MF 参数） |
| `mcu/EcuAbstraction/CanIf/config/CanIf_Cfg.c` | 自动生成的 CanIf PDU 配置 |
| `mcu/RTE/Rte_SignalAccess.c` | 自动生成的 `Rte_Read_xxx` / `Rte_Write_xxx` 函数 |

### 7.2 修改文件

| 文件 | 变更 |
|------|------|
| `mcu/RTE/Rte.c` | 从骨架实现 `Rte_Read_VehicleSignal()` → 内部调 `Com_ReceiveSignal()` + 物理转换 |
| `mcu/RTE/Rte.h` | 扩展：每个信号生成独立的 `Rte_Read_xxx()` / `Rte_Write_xxx()` 函数声明 |
| `mcu/RTE/Rte_Type.h` | 从 YAML/DBC 自动生成信号类型定义（已有骨架，需扩展） |
| `mcu/Services/Com/src/Com.c` | 从骨架实现 `Com_ReceiveSignal()` / `Com_SendSignal()`（只做 raw 拆装） |
| `mcu/Services/Com/include/Com.h` | 保持信号拆装接口不变（物理转换不在 Com 层） |
| `mcu/Services/Com/config/Com_Cfg.h` | 扩展 `Com_SignalConfigType`（加 `is_big_endian`） |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | ✅ 已实现 — 中断驱动 RX + 回调注册 + PDU 翻译 |
| `mcu/Services/CanTp/src/CanTp.c` | ✅ 已实现 — SF/FF/MF 分段 + FC 流控状态机 |
| `mcu/Services/PduR/src/PduR.c` | ✅ 已实现 — CanTp→Com 最小路由 |
| `mcu/App/Swc_SignalGateway/src/main.c` | 改用 `Rte_Read_xxx()` / `Rte_Write_xxx()` API（不再直接调 Com） |

### 7.3 废弃文件

| 文件 | 原因 |
|------|------|
| `mcu/MCAL/Can/config/Can_Cfg.h` 中的 `Can_SignalDefType` 及 `Can_SignalMap[]` | 阶段 2 完成后废弃，被 Com 配置表取代 |

---

## 8. 总结

```
┌──────────────────────────────────────────────────────────────────┐
│                     核心对比                                      │
├──────────────────────────────────────────────────────────────────┤
│  Can_SignalDefType:                                               │
│    扁平 struct — 信号描述 + CAN ID + 路由 + 物理转换 混在一起       │
│    靠数组索引访问 — 没有信号名                                      │
│    MCU 查表 → 转发 raw bytes — SoC 还要再解析一遍                  │
│                                                                   │
│  AUTOSAR 六层模型:                                                 │
│    SWC   — 强类型物理值写业务逻辑（不知道 CAN 存在）                 │
│    RTE   — SWC↔Com 映射 + 物理值转换(scale/offset) + 类型包装      │
│    Com   — 信号 ↔ I-PDU bit 级拆装（只认 raw 值）                  │
│    PduR  — PDU 跨模块路由（CanIf↔CanTp↔Com↔SpiIf）                  │
│    CanTp — N-PDU 分段/重组 + FC 流控（SF/FF/MF/CF）                 │
│    CanIf — PDU ↔ CAN 控制器/ID 映射                               │
│    信号有 ID + 名称 — Rte_Read_VehicleSpeed() → uint16_t km/h     │
│    MCU 完成全部拆装+物理转换 → SoC 只收命名物理值                    │
├──────────────────────────────────────────────────────────────────┤
│                     改造三步走（自底向上逐层实现）                       │
│                                                                   │
│  阶段 1 ✅: CanIf 层就位 — 中断驱动 RX + 回调注册 + PDU 翻译          │
│           → 详见 [3_CanIf_CAN接口层详解](./3_CanIf_CAN接口层详解.md)  │
│  阶段 2 ✅: PduR 最小路由 + CanTp 传输层 — PDU 路由 + SF/FF/MF 流控    │
│           → 详见 [5_CanTp_CAN传输层详解](./5_CanTp_CAN传输层详解.md)  │
│  阶段 3 ⏳: Com 信号层 + RTE — 信号拆装 + 物理值转换 + SWC 强类型接口   │
├──────────────────────────────────────────────────────────────────┤
│                     DBC 模拟                                       │
│                                                                   │
│  YAML 替代 DBC: 字段自由扩展、工具零依赖、一源多目标                  │
│  MCU 用全部字段 → 生成 C 配置数组 + RTE 函数                        │
│  SoC 只取 name/unit/scale/offset → 不接触 CAN 层信息                │
└──────────────────────────────────────────────────────────────────┘
```
