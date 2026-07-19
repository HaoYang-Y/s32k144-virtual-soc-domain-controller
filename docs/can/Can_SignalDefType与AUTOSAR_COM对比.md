# Can_SignalDefType 与 AUTOSAR COM 标准对比

> 对比当前自定义 `Can_SignalDefType` 与 AUTOSAR COM 的 ComSignal / ComIPdu 模型，
> 给出从"扁平 DBC 等价物"到"分层 AUTOSAR COM"的迁移路线。

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

所以 COM 标准把这个映射拆成了**四层**，每一层只做一件事：

```
┌───────────────────────────────────────────────────────────┐
│  应用层 (SWC)                                              │
│  Rte_Read_VehicleSpeed() → 拿到 "120.5 km/h"              │
│  SWC 只看到命名物理值，不知道 CAN、不知道 bit 位置           │
├───────────────────────────────────────────────────────────┤
│  信号层 (Com)              ┌─────────────────────────┐    │
│  Com_ReceiveSignal(        │ Com_SignalConfigType    │    │
│    SIGNAL_ID_VEHICLE_SPEED) │ .signal_id = 0x0001    │    │
│  → 从 I-PDU 中提取 bit 0~7  │ .ipdu_id   = 3         │    │
│                             │ .bit_position = 0      │    │
│                             │ .bit_size    = 8       │    │
│                             │ .byte_order  = Intel   │    │
│                             └─────────────────────────┘    │
├───────────────────────────────────────────────────────────┤
│  PDU 层 (Com)               ┌─────────────────────────┐    │
│  Com_IPduGroupStart(3)      │ Com_IPduConfigType      │    │
│  → 周期发送或事件发送         │ .ipdu_id    = 3         │    │
│                              │ .can_id     = 0x123     │    │
│                              │ .dlc        = 8         │    │
│                              │ .cycle_time = 100ms     │    │
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

### 2.2 各层类型的职责单一化

| 层 | 类型 | 职责 | 只关心 |
|----|------|------|--------|
| Com (信号) | `Com_SignalConfigType` | 信号 ↔ I-PDU 的 bit 级拆装 | `signal_id`, `bit_position`, `bit_size` |
| Com (PDU) | `Com_IPduConfigType` | I-PDU ↔ CAN ID 的映射 | `ipdu_id`, `can_id`, `dlc`, `cycle_time` |
| PduR (路由) | `PduR_RouteConfigType` | PDU 在模块间路由 | `src_pdu_id`, `dst_pdu_id`, `src_module`, `dst_module` |
| CanIf (接口) | `CanIf_PduConfigType` | PDU ↔ 硬件实例映射 | `pdu_id`, `controller_id`, `can_id`, `dlc` |

**关键差异**：AUTOSAR 把 `Can_SignalDefType` 的 9 个字段拆分到了 4 个独立的配置表中，每个表只关注自己的抽象层级。

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
| `scale_num` | Com 或 SWC | — | **缺失** ⚠️ | AUTOSAR COM 标准本身不做物理值转换（它是信号拆装层，不是换算层） |
| `scale_den` | Com 或 SWC | — | **缺失** ⚠️ | 同上，物理转换可放 Com 的扩展字段，或放在上层 RTE / SWC |
| `offset` | Com 或 SWC | — | **缺失** ⚠️ | 同上 |
| `target_channel` | PduR | `PduR_RouteConfigType`（`src_module` / `dst_module`） | **已有** ✅ | PduR 的路由表就是做这个的：把 PDU 从 CanIf 路由到 SpiIf 或其他模块 |
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

// 物理值转换可以另建一个类型，不混入信号拆装层：
typedef struct {
    uint16_t signal_id;
    uint16_t scale_num;      // 物理值 = raw * scale_num / scale_den + offset
    uint16_t scale_den;
    int16_t  offset;
    char     unit[8];        // 如 "km/h"
} Com_SignalPhysicalType;    // 可选：放在 Com 层或 RTE/SWC 层
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

SoC 侧加载同一份 YAML，但**只关心物理层信息**：

```cpp
// SoC 加载后：
// can_id / start_bit / byte_order — 直接丢弃，SoC 不需要
// name / unit / scale / offset   — 用来生成 CanSignalDef
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
CanIf_RxIndication() → PduR
  │
  ▼
Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED)
  │ 查 Com_SignalConfig[0]: ipdu_id=3, bit_position=0, bit_size=8
  │ 从 I-PDU 3 的 data[] 中提取 bit 0~7 → raw=0x11
  │ 查 Com_SignalPhysical[0]: raw*1/1+0 → 物理值=17
  ▼
Rte_Read_VehicleSpeed() → "VehicleSpeed = 17 km/h"
```

关键区别：

| | 阶段 1 (Can_SignalDefType) | 阶段 2 (AUTOSAR Com) |
|---|---|---|
| 信号命名 | ❌ 靠数组索引 | ✅ `COM_SIGNAL_ID_VEHICLE_SPEED` 宏 |
| 信号拆装 | 手写代码循环查表 | `Com_ReceiveSignal()` 标准 API |
| I-PDU 概念 | ❌ 无 | ✅ 信号打包成 I-PDU → 统一收发 |
| 物理转换 | 耦合在 struct 里 | 独立 `Com_SignalPhysicalType` 表 |
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

数据流变成完全标准化的 AUTOSAR 路径：

```
CAN 帧
  → Can_Read()
  → CanIf_RxIndication()
  → PduR_CanIfRxIndication()  查 PduR_RouteConfig → dst_module=SPIIF
  → SpiIf 发送给 SoC
  → SoC 收到: "VehicleSpeed = 17 km/h" （物理值，不含任何 CAN 信息）
```

### 4.4 三阶段汇总

```
阶段 1（当前可行）                  阶段 2（目标）                   阶段 3（完整）
─────────────────                  ────────────                   ────────────
signals.yaml                       signals.yaml                   signals.yaml
  │                                  │                              │
  ▼                                  ▼                              ▼
Can_SignalDefType[]                Com_SignalConfig[]             Com_SignalConfig[]
(扁平数组, 不改代码)               Com_IPduConfig[]               Com_IPduConfig[]
                                   CanIf_PduConfig[]              CanIf_PduConfig[]
                                   Com_SignalId.h                 PduR_RouteConfig[]
                                                                  Com_SignalId.h
                                                                  Com_SignalPhysical[]

Can_Read() + 手写循环查表           Com_ReceiveSignal()            同阶段2 + PduR 自动路由
                                   Com_SendSignal()
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
    can_id/start_bit/   name/unit/         可读的信号清单
    length/byte_order   scale/offset
    scale/offset/cycle  （丢弃 CAN 字段）

    生成:               加载为:
    Com_SignalConfig[]  CanSignalDef[]
    Com_IPduConfig[]    （仅物理层）
```

**核心原则**：
- YAML 包含**完整的**信号描述（CAN 层 + 物理层）
- MCU 侧代码生成器**使用全部字段**做信号拆装
- SoC 侧加载器**只取 name/unit/scale/offset**——因为 SoC 收到的已经是 MCU 解析好的物理值
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
    unit: string             # 单位（仅 SoC / 文档需要）
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

    # 2. 生成 Com_Cfg.c 中的数组定义
    write_com_signal_config(ipdus, output_dir)
    write_com_ipdu_config(ipdus, output_dir)
    write_canif_pdu_config(ipdus, output_dir)
    write_signal_id_macros(ipdus, output_dir)
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

### 6.2 改造后（AUTOSAR Com）

```
  CAN 帧到达 (ID=0x123)
       │
       ▼
  Can_Read() → Can_PduType
       │
       ▼
  CanIf_RxIndication() → PduR
       │
       ▼
  Com_MainFunctionRx():
    Com_ReceiveSignal(COM_SIGNAL_ID_VEHICLE_SPEED)
    Com_ReceiveSignal(COM_SIGNAL_ID_ENGINE_RPM)
    Com_ReceiveSignal(COM_SIGNAL_ID_COOLANT_TEMP)
       │
       ├── "VehicleSpeed" = 17 km/h    ← 命名物理值
       ├── "EngineRPM"    = 3200 rpm
       └── "CoolantTemp"  = 85 degC
       │
       ▼
  PduR 路由 → SpiIf 发送
       │
       ▼
  SoC 收到:
    {"VehicleSpeed": {"value": 17, "unit": "km/h"},
     "EngineRPM":    {"value": 3200, "unit": "rpm"},
     "CoolantTemp":  {"value": 85, "unit": "degC"}}
    ← 只有命名物理值，零 CAN 信息!
```

**改进**：
- SoC 不再需要知道 CAN ID、start_bit、byte_order
- 同一个 YAML 的物理层字段（name/unit/scale）直接给 SoC 用
- MCU 和 SoC 的解析逻辑不重复

---

## 7. 涉及的文件清单

### 7.1 新增文件

| 文件 | 内容 |
|------|------|
| `mcu/Config/signals.yaml` | 信号矩阵定义（单一数据源） |
| `mcu/Tools/generate_autosar_com_cfg.py` | 从 YAML 生成 C 配置数组 |
| `mcu/Services/Com/config/Com_Cfg.c` | 自动生成的信号/IPDU 配置 |
| `mcu/Services/Com/config/Com_SignalId.h` | 自动生成的信号 ID 宏 |
| `mcu/EcuAbstraction/CanIf/config/CanIf_Cfg.c` | 自动生成的 CanIf PDU 配置 |

### 7.2 修改文件

| 文件 | 变更 |
|------|------|
| `mcu/Services/Com/src/Com.c` | 从骨架实现 `Com_ReceiveSignal()` / `Com_SendSignal()` |
| `mcu/Services/Com/include/Com.h` | 扩展类型定义（加 `Com_SignalPhysicalType`） |
| `mcu/Services/Com/config/Com_Cfg.h` | 扩展 `Com_SignalConfigType`（加 `is_big_endian`） |
| `mcu/EcuAbstraction/CanIf/src/CanIf.c` | 从骨架实现 `CanIf_Transmit()` / `CanIf_RxIndication()` |
| `mcu/Services/PduR/src/PduR.c` | 从骨架实现路由逻辑（阶段 3） |
| `mcu/App/Swc_SignalGateway/src/main.c` | 改用 `Com_ReceiveSignal()` + `Com_SendSignal()` API |

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
│    扁平 struct — 信号描述 + CAN ID + 路由混在一起                   │
│    靠数组索引访问 — 没有信号名                                      │
│    MCU 查表 → 转发 raw bytes — SoC 还要再解析一遍                  │
│                                                                   │
│  AUTOSAR COM:                                                     │
│    分四层 — Com信号层 / ComPDU层 / CanIf接口层 / Can硬件层           │
│    信号有 ID + 名称 — Com_ReceiveSignal(COM_SIGNAL_ID_XXX)          │
│    MCU 完成全部拆装 → SoC 只收命名物理值                            │
├──────────────────────────────────────────────────────────────────┤
│                     改造三步走                                     │
│                                                                   │
│  阶段 1: YAML → Can_SignalMap[] (零运行时改动，只改配置来源)         │
│  阶段 2: YAML → Com_SignalConfig[] + Com_IPduConfig[] (核心)       │
│  阶段 3: 启用 PduR 路由层 (完整 AUTOSAR 对齐)                       │
├──────────────────────────────────────────────────────────────────┤
│                     DBC 模拟                                       │
│                                                                   │
│  YAML 替代 DBC: 字段自由扩展、工具零依赖、一源多目标                  │
│  MCU 用全部字段 → 生成 C 配置数组                                   │
│  SoC 只取 name/unit/scale/offset → 不接触 CAN 层信息                │
└──────────────────────────────────────────────────────────────────┘
```
