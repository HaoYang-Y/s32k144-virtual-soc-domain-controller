# Com — AUTOSAR 通信层详解

> 这是 AUTOSAR CAN 通信栈系列的第 9 篇。
> 讲解 COM（Communication）层——信号与 I-PDU 的映射、位级编解码、Update Bit 模式、超时检测。
> 适合刚学完前 5 篇的嵌入式小白和 AUTOSAR 小白。

---

## 先读这个：COM 层到底是干什么的？

打个比方。你有一堆零件（**信号**）要运到目的地，但你只有一辆卡车（**CAN 帧**）。你不会每件零件单独发一趟车吧？你会：

1. 把零件按大小、位置码好放进箱子里（**打包** `Com_PackSignal`）
2. 等箱子装满或到发车时间了再发走（**发送调度** `Com_MainFunction`）
3. 收货时从箱子里把零件取出来（**解包** `Com_UnpackSignal`）

这就是 COM 层做的事。它是 AUTOSAR 通信栈的**最顶层**——往上面对 SWC（应用层），往下连接 PduR（PDU 路由器）。

```
    SWC (应用层)
      │  Rte_Write_VehicleSpeed()  ← 业务代码调这个
      ▼
    COM (通信层)    ← ★ 本文主角
      │  Com_SendSignal() / Com_MainFunction()
      ▼
    PduR (路由)
      │
      ▼
    CanTp/CanIf/Can → 物理总线
```

---

## 1. COM 层核心概念（请先搞明白这三个）

### 1.1 信号 (Signal) — 最小的数据单元

一个"信号"就是 SWC 操作的最小数据单元：

| 信号名 | 类型 | 含义 |
|--------|------|------|
| TestTxCounter | uint32 | 测试计数器，4 字节 |
| TestTxMagic0 | uint8 | 魔数 0xAA，1 字节 |
| 车速 | uint16 | 0.01 km/h 分辨率 |

SWC 不需要知道这个信号在 CAN 帧的第几个字节——那是 COM 层配置的事。

### 1.2 I-PDU (Interaction Layer PDU) — 装信号的"集装箱"

多个信号打包进一个 I-PDU，映射到一条 CAN 帧：

```
I-PDU 0x123 (TX, DLC=6, 周期 500ms):
┌────────────┬──────────┬──────────┐
│ byte 0-3   │ byte 4   │ byte 5   │
│ Counter    │ Magic0   │ Magic1   │
│ 32-bit     │ 8-bit    │ 8-bit    │
│ TRIGGERED  │ PENDING  │ NONE     │
└────────────┴──────────┴──────────┘

I-PDU 0x100 (RX, DLC=7, 事件触发):
┌─────────────────────────────────┐
│         byte 0-6                │
│         RxData                  │
│         64-bit                  │
│         Update Bit + Timeout    │
└─────────────────────────────────┘
```

### 1.3 Shadow Buffer — 为什么 Com_SendSignal 不直接发？

```c
// SWC 写信号
Com_SendSignal(COUNTER, &value);  // 只写 Shadow Buffer + I-PDU 缓冲区
// ... CAN 帧没发！只是准备好了 ...
// ... 等 Com_MainFunction 被调用的时候才真正发 ...
```

**为什么不立刻发？** 因为同一条 CAN 帧上可能有 3 个信号。如果每个信号写的时候都发一次，那就发了 3 条帧——浪费总线带宽。COM 的策略是"攒着，到时间（或触发条件满足）再发"。

---

## 2. COM 层数据结构

### 2.1 信号配置 `Com_SignalConfigType`

```c
typedef struct {
    Com_SignalIdType        signal_id;          // 信号 ID (0, 1, 2, 3...)
    Com_IPduIdType          ipdu_id;            // 属于哪个 I-PDU
    uint8_t                 bit_position;       // 起始位 (0 = byte0 的 bit0)
    uint8_t                 bit_size;           // 位宽 (1~64)
    Com_ByteOrderType       byte_order;         // Intel 小端 或 Motorola 大端
    uint8_t                 is_signed;          // 有符号? (0/1)
    Com_TransferPropertyType transfer_property; // TRIGGERED / PENDING / NONE
    uint8_t                 has_update_bit;     // 启用 Update Bit?
    uint16_t                timeout_ms;         // 超时时间 (ms), 0=不检测
} Com_SignalConfigType;
```

**bit_position 是什么意思？**

AUTOSAR 中，位从字节 0 的最低有效位（bit 0）开始编号：

```
CAN 帧数据区 (8 bytes):
byte[0] byte[1] byte[2] byte[3] byte[4] byte[5] byte[6] byte[7]
│       │       │       │       │       │       │       │
bit0-7  bit8-15 bit16-23 bit24-31 bit32-39 bit40-47 bit48-55 bit56-63
```

比如 `bit_position=0, bit_size=32` → 信号占 byte[0]~byte[3]，在 CAN 帧的最前面 4 字节。

### 2.2 I-PDU 配置 `Com_IPduConfigType`

```c
typedef struct {
    Com_IPduIdType  ipdu_id;        // I-PDU ID
    Com_IPduIdType  group_id;       // 所属 IPduGroup
    uint8_t         dlc;            // 数据长度 (1~8)
    uint32_t        can_id;         // CAN ID (文档用途)
    uint16_t        cycle_time_ms;  // 周期 (ms), 0=事件触发
} Com_IPduConfigType;
```

### 2.3 I-PDU 运行时状态

```c
typedef struct {
    uint8_t  data[8];        // I-PDU 字节数组 ← 信号的"集装箱"
    uint32_t last_tx_ms;     // 上次发送时间
    uint8_t  dirty;          // 脏标记: 有信号更新了但没发
    uint8_t  enabled;        // IPduGroup 是否启用
} Com_IPduBufferType;
```

---

## 3. 信号编解码——COM 层最核心的算法

### 3.1 Intel 格式 (小端，LSB first)

S32K144 是小端 CPU。Intel 格式**直接匹配 CPU 字节序**，是最常用的格式。

```
例子: 32-bit 值 = 0x01020304, bit_position=0

Intel 存储 (LSB 在低地址):
  byte[0] = 0x04  ← LSB
  byte[1] = 0x03
  byte[2] = 0x02
  byte[3] = 0x01  ← MSB
```

实现原理（逐位写入）：
```c
for (i = 0; i < bit_size; i++) {
    target_bit   = bit_position + i;          // 第 i 位写到哪个总线位
    target_byte  = target_bit / 8;            // 哪个字节
    target_mask  = 1 << (target_bit % 8);     // 字节内哪个 bit
    if (value & (1ULL << i))
        buffer[target_byte] |= target_mask;   // 写 1
    else
        buffer[target_byte] &= ~target_mask;  // 写 0
}
```

### 3.2 Motorola 格式 (大端，MSB first)

跨字节时，**高位在低地址**——和 Intel 刚好相反。

```
例子: 32-bit 值 = 0x01020304, bit_position=0

Motorola 存储 (MSB 在低地址):
  byte[0] = 0x01  ← MSB
  byte[1] = 0x02
  byte[2] = 0x03
  byte[3] = 0x04  ← LSB
```

**重要：单字节信号（bit_size ≤ 8）两种格式结果相同！** 只有跨字节才有区别。

### 3.3 有符号扩展（Sign Extension）

读出来的值如果是负数，需要把高位全填成 1：
```c
if (is_signed && (value & sign_bit_mask)) {
    value |= ~((1ULL << bit_size) - 1ULL);  // 高位全填 1
}
```

---

## 4. 发送时机——Transfer Property 三种模式

同一个 I-PDU 里可以有三种不同触发模式的信号：

```
TestTxCounter (TRIGGERED):  "我在变了，立刻发！"
TestTxMagic0  (PENDING):    "我变了，标记一下，等 MainFunction 一起发"
TestTxMagic1  (NONE):       "我不管发不发，等 500ms 周期到了自然会发"
```

**发送规则**：
- 任一 TRIGGERED 信号改变 → 立即发送整个 I-PDU（不等 MainFunction）
- 无 TRIGGERED 但有 PENDING 改变 → MainFunction 中发送
- 全 NONE → 仅按 cycle_time_ms 周期发送

```c
void Com_MainFunction(void) {
    for each I-PDU:
        // 1. 周期发送
        if (elapsed >= cycle_time_ms):
            Com_TriggerIPduSend()

        // 2. PENDING 信号触发
        else if (dirty && has_pending_signal):
            Com_TriggerIPduSend()
}
```

---

## 5. Update Bit 模式——AUTOSAR 的精髓

### 5.1 问题：SWC 怎么知道"有新数据"？

**不好**的做法：比较新旧值。
```c
if (new_value != old_value) {
    // 有问题! 值从 1→2→1, 新旧都是 1, 你以为没变, 其实变过!
}
```

**AUTOSAR 的做法**：用 Update Bit。
```c
void Com_RxIndication(...) {
    // COM 收到新 I-PDU → 解包信号 → 设 Update Bit = 1
    Com_UpdateBits[signal_id] = 1;
}

// SWC 轮询
void Swc_MainFunction(void) {
    if (Com_GetUpdateBit(VEHICLE_SPEED)) {  // 有新数据吗？
        Com_ReceiveSignal(VEHICLE_SPEED, &speed);
        // 处理 speed...
        // GetUpdateBit 已自动清 0
    }
}
```

### 5.2 `Com_GetUpdateBit` 的行为

- 返回当前 Update Bit 值（1=有新数据，0=无）
- **读取后自动清 0**（下次收到新数据才再置 1）
- 如果信号配置中 `has_update_bit = 0`，永远返回 0

---

## 6. Deadline Monitoring——数据不能太老

对于安全关键信号，**"没数据"比"数据值异常"更危险**。

```
RX 信号 timeout_ms=3000 (3秒)

时间线:
t=0    收到数据 → COM_SIGNAL_OK (红灯灭)
t=1s   (没有新数据)
t=2s   (没有新数据)
t=3s   Com_MainFunction 检测到超时 → COM_SIGNAL_TIMEOUT (红灯亮!)
t=4s   收到新数据 → COM_SIGNAL_OK (红灯灭)
```

SWC 在读信号前应该先检查：
```c
if (Com_GetSignalStatus(BRAKE_POS) == COM_SIGNAL_TIMEOUT) {
    ActivateSafeMode();  // 刹车信号超时 → 进入安全模式
} else {
    Com_ReceiveSignal(BRAKE_POS, &position);
    NormalOperation(position);
}
```

---

## 7. DET 错误报告

COM 层在 API 入口处检查参数合法性：

| 错误码 | 含义 | 触发条件 |
|--------|------|----------|
| `COM_E_UNINIT` | 未初始化 | 在 `Com_Init()` 之前调 API |
| `COM_E_PARAM` | 参数非法 | 传 NULL 指针 |
| `COM_E_SIGNAL_NOT_FOUND` | 信号不存在 | 传不存在的 SignalId |

错误通过 `Det_ReportError()` 输出到 UART：
```
[Com] DET Err (API=1, Err=4)  ← API 1=SendSignal, Err 4=信号未找到
```

生产版本中 DET 宏被替换为空——零开销。

---

## 8. IPduGroup——通信模式开关

ILduGroup 用来控制一组 I-PDU 的收发开关。典型场景：

```
正常模式: IPduGroup_ALL 启用 → 所有 I-PDU 正常收发
诊断模式: IPduGroup_ALL 停止 + IPduGroup_DIAG 启用 → 只收发诊断 PDU
刷写模式: IPduGroup_BOOT 启用 → 只收发 Bootloader PDU
```

```c
Com_IPduGroupStop(COM_IPDU_GROUP_ALL);      // 停掉所有正常通信
Com_IPduGroupStart(COM_IPDU_GROUP_DIAG);    // 只开诊断通信
```

---

## 9. 完整收发数据流

### 9.1 TX: 信号 → CAN 总线

```
SWC 调用 Rte_Write(Counter, value=0x5B)
  │
  ▼
RTE: Rte_Write_VehicleSignal() → Com_SendSignal(SIGNAL_ID, &value)
  │
  ▼
COM_Com_SendSignal():
  1. memcpy value 到 Shadow Buffer   ← uint32→uint64
  2. Com_PackSignal()→I-PDU buffer   ← bit 级 Intel 打包
  3. I-PDU dirty = 1
  4. transfer_property==TRIGGERED? → Com_TriggerIPduSend()
     ├→ Com_TriggerIPduSend() 构建 PduInfoType
     └→ PduR_ComTransmit() → CanTp → CanIf → Can → 总线
```

### 9.2 RX: CAN 总线 → 信号

```
CAN 帧 0x100#07AABBCCDDEEFF00 到达
  │
  ▼
硬件 ISR → CanIf → PduR → CanTp_RxIndication:
  ├→ PCI 解码: 0x07 = SF(7 字节数据)
  ├→ 跳过 PCI 字节: SduDataPtr = AA BB CC DD EE FF 00
  ├→ SduLength = 7
  └→ PduR_CanTpRxIndication()
       │
       ▼
     Com_RxIndication():
       1. memcpy 7 字节到 I-PDU buffer
       2. 找出该 I-PDU 的所有信号 (TestRxData)
       3. Com_UnpackSignal() → 0x00FFEEDDCCBBAA
       4. 写入 Shadow Buffer + Update Bit=1 + Status=OK
  │
  ▼
SWC 轮询:
  if (Com_GetUpdateBit(RX_DATA)):       ← 1→有新数据
     Com_ReceiveSignal(RX_DATA, &rx):   ← 读 Shadow Buffer
     处理 rx...
```

---

## 10. 项目实测验证

### 发送测试

```
candump can0:
  can0  123   [8]  06 03 00 00 00 AA 55 00
                   PCI SF   ← I-PDU 数据 →

  解析:
    Counter  = 0x00000003 (Intel, byte0-3) ✅
    Magic0   = 0xAA (byte4) ✅
    Magic1   = 0x55 (byte5) ✅
```

### 接收测试

```
cansend can0 100#07AABBCCDDEEFF00
  → MCU ACK ✓
  → Com_RxIndication 解包 TestRxData
  → Update Bit = 1
  → 蓝灯闪烁 (50ms) ← 人眼可见!
  → SignalStatus = COM_SIGNAL_OK

等 3 秒不发 → 红灯亮 = COM_SIGNAL_TIMEOUT ← 超时触发!
再发一帧   → 红灯灭 = 恢复正常
```

---

## 11. AUTOSAR 小白常见问题

**Q: COM 层和 RTE 有什么区别？**
A: RTE 是 SWC 和 BSW 之间的胶水层——只管"把 SWC 的调用转给 BSW"。COM 是做实际工作的——打包信号、调度发送、检测超时。RTE 是通道，COM 是引擎。

**Q: 为什么配置表是 const？**
A: AUTOSAR 的哲学是"编译时决定一切"。配置存在 Flash（ROM）里，不占 RAM。运行时只读、只查表。这避免了动态内存分配的所有问题。

**Q: 我能加一个新信号吗？**
A: 三步走：(1) 在 `Com_Cfg.h` 加 `COM_SIGNAL_ID_xxx`；(2) 在 `Com_Cfg.c` 加信号配置（bit_position, bit_size 等）；(3) 调 `Com_SendSignal(新ID, &value)`。不需要改任何 Com.c 核心代码。

**Q: Intel 和 Motorola 怎么记？**
A: Intel = **低**位在**低**地址 → "**低低**"。Motorola = **高**位在**低**地址 → "**高低**"。S32K144 是小端 CPU，所以 Intel 格式就是 CPU 原生态——最常用。

---

## 参考文件

| 文件 | 内容 |
|------|------|
| `mcu/Services/Com/include/Com.h` | COM API 声明 |
| `mcu/Services/Com/src/Com.c` | COM 完整实现（~950 行） |
| `mcu/Services/Com/config/Com_Cfg.h` | 信号/I-PDU 配置结构 |
| `mcu/Services/Com/config/Com_Cfg.c` | 实际配置数据 |
| `mcu/Services/Det/include/Det.h` | DET API |
| `mcu/Services/Det/src/Det.c` | DET 实现 |
| `mcu/RTE/Rte.c` | RTE→COM 桥接 |
| `mcu/App/Swc_SignalGateway/src/main.c` | 完整链路测试程序 |
