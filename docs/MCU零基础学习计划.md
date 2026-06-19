# 🎯 学习计划 — 车载信号网关（基于 S32K144 + Ubuntu SOC）

> **目标人群**: 有 Linux C++ 基础，嵌入式 C 经验较少
> **预计时长**: 8~12 周（每周 5~8 小时）
> **核心产出**: GPIO/ADC 信号采集 → MCU CP 四层处理 → SPI 差分帧 → SOC AP 三层处理 → SOME/IP + CAN 发布
> **原则**: 自底向上，先独立学 MCAL 外设驱动，再逐层上推至 SWC 和 SOC 服务
> 
> **当前状态 (2026-06-19)**: MCU 端各层骨架代码已搭好，CAN 和 UART 尚未调通。

---

## 🧩 开发方式说明

本工程 MCU 代码使用 **NXP S32 SDK for S32K1xx (RTM 4.0.2)** 的 DRV 层 API 开发，
遵循 AUTOSAR CP 命名规范（`Xxx_Init`、`Xxx_Read`、`Xxx_Write`）。MCAL 层各模块封装 SDK API，
上层通过 AUTOSAR 标准接口调用。

### MCU 分层架构速览

```
App/Swc_SignalGateway/    ← ⑤ SWC 应用层（周期调度，不碰硬件）
RTE/                       ← ④ 运行时环境（volatile 共享内存，零开销宏）
EcuAbstraction/            ← ③ ECU 抽象层（CanIf, IoHwAb, SpiIf）
CDD/Uart/                  ← ② 复杂驱动（UART 日志）
MCAL/                      ← ① 微控制器抽象层（Gpio, Mcu, Adc, Can, Spi, Port）
  每模块: include/ src/ config/
NXP S32 SDK                ← 底层（芯片头文件 + 启动代码 + 链接脚本）
```

### SDK 头文件路径速查

| 外设 | SDK 头文件（相对 SDK 根目录） | 常用 API 前缀 |
|------|------------------------------|--------------|
| FlexCAN | `platform/drivers/inc/flexcan_driver.h` | `FLEXCAN_DRV_*` |
| LPUART | `platform/drivers/inc/lpuart_driver.h` | `LPUART_DRV_*` |
| Pins (GPIO) | `platform/drivers/inc/pins_driver.h` | `PINS_DRV_*` |
| LPIT | `platform/drivers/inc/lpit_driver.h` | `LPIT_DRV_*` |
| ADC | `platform/drivers/inc/adc_driver.h` | `ADC_DRV_*` |
| LPSPI | `platform/drivers/inc/lpspi_driver.h` | `LPSPI_DRV_*` |
| Clock | `platform/drivers/inc/clock_manager.h` | `CLOCK_SYS_*` |

---

## ⚡ 硬件拓扑

```
PC (宿主机)
├── Ubuntu 24.04 VM
│   ├── Signal Bridge Daemon（vsomeip + libmpsse）
│   └── FT2232H（USB 直通，SPI Slave）
│
└── S32K144 开发板（MCU）
    ├── LPSPI0 ──── SPI Master → FT2232H
    ├── FlexCAN0 ── CAN → USB-CAN 分析仪
    ├── LPUART1 ── UART → 宿主机串口（调试日志）
    ├── GPIO 按键 ×5（车门×4 / 档位 P/R/N/D）
    └── ADC 旋钮 ×2（方向盘角度 / 车速）
```

> MCU 通过 SPI 将信号帧传输给 SOC，同时通过 CAN 发布到其他域控制器。
> SPI 使用 32B 固定帧 + 差分协议，详见 [VehicleGateway_Design.md](VehicleGateway_Design.md)。

---

## 学习路线总览

```
阶段 0：环境搭建与硬件确认                     ← 让 SPI/CAN/UART 物理连通
        ↓
阶段 1：MCAL 外设驱动（Gpio→Mcu→Adc→Can→Spi→Port）
        ↓         每模块独立学习，SDK API 开发
阶段 2：ECU 抽象层（CanIf→IoHwAb→SpiIf）+ CDD/Uart
        ↓         把 MCAL 接口抽象为硬件无关的 BSW 接口
阶段 3：RTE + SWC 信号采集（共享缓冲区 + 周期调度）
        ↓         零开销宏连接 SWC 和底层
阶段 4：CAN 通信（FlexCAN 收发 + CAN 分析仪验证）
        ↓         附 CP Can/CanIf/PduR/Com 概念
阶段 5：SPI 通信（32B 固定帧 + 差分 + CRC8）
        ↓         附 CP Spi/Com Stack 概念
阶段 6：SOME/IP 服务通信（vsomeip + SD）
        ↓         附 AP ara::com/ara::sd 概念
阶段 7：UDS 诊断（后续扩展）
```

---

## 阶段 0：环境搭建与硬件确认（1 天）

### 硬件确认

- 确认 S32K144 开发板供电正常
- 确认 FT2232H USB-SPI 桥被 Ubuntu 虚拟机识别（USB 直通）
- 确认 USB-CAN 工具被 Ubuntu 虚拟机识别
- 确认 USB-UART 串口被宿主机识别（用于 MCU 调试日志）
- CAN 总线两端接 **120Ω 终端电阻**

### 虚拟机侧驱动加载

```bash
# CAN 驱动
sudo modprobe gs_usb   # 或 slcan / peak_usb
sudo ip link set can0 up type can bitrate 500000

# FT2232H 驱动（libmpsse 依赖 libftdi1）
sudo apt install libftdi1-dev

# 验证
candump can0
```

### 开发环境确认

```bash
# MCU 交叉编译
sudo apt install gcc-arm-none-eabi make
arm-none-eabi-gcc --version

# SOC 构建
sudo apt install cmake g++
cmake --version
```

---

## 阶段 1：MCAL 外设驱动（2~3 周）

> **目标**: 按 AUTOSAR CP MCAL 规范实现外设驱动，使用 NXP SDK DRV 层 API。

### 子任务

| 模块 | 目录 | 核心 API |
|------|------|---------|
| Mcu | `mcu/MCAL/Mcu/` | `Mcu_InitClock()`、`Mcu_GetCoreFreq()` |
| Port | `mcu/MCAL/Port/` | `Port_SetPinMode()`、`Port_SetMuxMode()` |
| Gpio | `mcu/MCAL/Gpio/` | `Gpio_Init()`、`Gpio_ReadPin()`、`Gpio_WritePin()` |
| Adc | `mcu/MCAL/Adc/` | `Adc_Init()`、`Adc_ReadGroup()` |
| Can | `mcu/MCAL/Can/` | `Can_Init()`、`Can_Transmit()`、`Can_Receive()` |
| Spi | `mcu/MCAL/Spi/` | `Spi_Init()`、`Spi_WriteIb()`、`Spi_ReadIb()` |

> 每个模块都遵循 `include/`（接口头文件）+ `src/`（实现）+ `config/`（配置参数）三目录结构。

### C 语言嵌入式必备知识点

| 知识点 | 为什么重要 | 本项目应用 |
|--------|-----------|-----------|
| **typedef / 结构体** | 封装配置参数 | `Gpio_ConfigType`、`Can_ConfigType` |
| **枚举与宏定义** | 定义模式/状态 | `GPIO_LEVEL_LOW`、`CAN_BAUDRATE_500K` |
| **volatile 关键字** | 防止编译器优化 | 共享缓冲区、SDK 状态结构体 |
| **函数指针** | 中断回调（阶段 2 用） | ISR 注册 |
| **链接脚本** (.ld) | 内存布局 | SDK 官方 `S32K144_64_flash.ld` |

### 阶段 1 验证

```bash
cd mcu && make
# 预期：源文件编译成功，生成 mcu_demo.elf
```

**🔄 AUTOSAR CP 概念穿插**: MCAL 层是 AUTOSAR CP 的最底层，所有外设驱动通过标准接口（`Xxx_Init`、`Xxx_Read`/`Xxx_Write`）向上层暴露。这一层的规范保证了上层代码不依赖具体芯片。

---

## 阶段 2：ECU 抽象层 + CDD（1~2 周）

> **目标**: 在 MCAL 之上构建硬件无关的抽象接口。

### 子任务

| 模块 | 目录 | 核心 API |
|------|------|---------|
| CanIf | `mcu/EcuAbstraction/CanIf/` | `CanIf_Transmit()` 封装 `Can_Transmit()` |
| IoHwAb | `mcu/EcuAbstraction/IoHwAb/` | `IoHwAb_ReadPin()` 封装 `Gpio_ReadPin()` |
| SpiIf | `mcu/EcuAbstraction/SpiIf/` | `SpiIf_WriteIb()` 封装 `Spi_WriteIb()` |
| Uart | `mcu/CDD/Uart/` | `Uart_Init()`、`Uart_SendString()` (LPUART SDK API) |

> 这层的目的是解耦：上层 SWC 不知道底层是哪个 CAN 控制器、哪个 SPI 外设。

### 阶段 2 验证

```bash
cd mcu && make
# 预期：各抽象层模块编译通过，能调用对应 MCAL 接口
```

**🔄 AUTOSAR CP 概念穿插**: ECU Abstraction Layer 让上层软件组件（SWC）不依赖具体 ECU 硬件。
CanIf 屏蔽了 CAN 控制器的差异，IoHwAb 屏蔽了 GPIO/ADC 的引脚差异。

---

## 阶段 3：RTE + SWC 信号采集（1~2 周）

> **目标**: 构建运行时环境和 SWC 调度框架。

### 共享缓冲区（RTE 模拟）

```c
/* Rte_Type.h — 共享信号类型定义 */
typedef struct {
    uint8_t  door_status;      /* 车门×4 */
    uint8_t  gear_position;    /* 档位 P/R/N/D */
    uint8_t  headlight;        /* 大灯 */
    uint8_t  turn_signal;      /* 转向灯 */
    uint8_t  horn;             /* 喇叭 */
    int16_t  steering_angle;   /* 方向盘角度 */
    uint16_t vehicle_speed;    /* 车速 */
} SharedSignalsType;

/* Rte.h — 零开销 RTE 宏 */
#define Rte_Read(sig)   g_shared_signals.sig
#define Rte_Write(sig, val)  g_shared_signals.sig = (val)

/* Rte.c — 全局共享缓冲区实例 */
volatile SharedSignalsType g_shared_signals;
```

### 子任务

- [ ] `mcu/RTE/Rte_Type.h` — 共享信号类型定义
- [ ] `mcu/RTE/Rte.h` — Rte_Read/Rte_Write 宏
- [ ] `mcu/RTE/Rte.c` — volatile 共享缓冲区实例
- [ ] `mcu/App/Swc_SignalGateway/src/Swc_SignalGateway.c` — SWC 周期调度
- [ ] GPIO 中断 → ISR Rte_Write（写共享缓冲区）
- [ ] ADC PIT 定时 → ISR Rte_Write
- [ ] SWC_Run → Rte_Read → 差分编码 → CanIf/SpiIf 发送

### 阶段 3 验证

```
MCU 端 (S32K144)
  按键按下 → GPIO ISR → Rte_Write → 更新 g_shared_signals
  ADC 旋钮 → PIT ISR → Rte_Write → 更新 g_shared_signals
  Swc_SignalGateway_Run → Rte_Read → Uart 打印
```

**🔄 AUTOSAR CP 概念穿插**: RTE（运行时环境）是 AUTOSAR CP 中 SWC 之间的通信中枢。
本工程用 `volatile` 共享内存 + 宏模拟 —— ISR 写、SWC 读，零开销。

---

## 阶段 4：CAN 通信（2~3 周）

> **目标**: FlexCAN 收发 CAN 帧，USB-CAN 分析仪捕获验证。

### CAN 信号矩阵

| 信号 | CAN ID | 周期 | 数据类型 | 范围 |
|------|--------|------|---------|------|
| 车门状态 | 0x100 | 100ms | u8[0..3] | 0/1 |
| 档位 | 0x110 | 变化 | u8[4..6] | 0=P,1=R,2=N,3=D |
| 大灯 | 0x120 | 变化 | u8[0..1] | 0=关,1=近光,2=远光 |
| 转向灯 | 0x121 | 变化 | u8[2..3] | 0=关,1=左,2=右 |
| 喇叭 | 0x130 | 变化 | u8[0] | 0/1 |
| 方向盘角度 | 0x200 | 50ms | i16 LSB | -450°~+450° |
| 车速 | 0x201 | 100ms | u16 LSB | 0~300 km/h |

### 验证方法

```
S32K144 FlexCAN0                  Ubuntu 虚拟机
  按键 → 编码 CAN 帧 → 发送 →  candump can0 捕获
  candump 发送 →       接收 →  UART 打印
```

✅ **完成标准**: S32K144 能发送/接收 CAN 帧；USB-CAN 工具 `candump` 双向验证通过。

**🔄 AUTOSAR CP 概念穿插**: Can→CanIf→PduR→Com 四层 —— Can 管硬件、CanIf 管接口抽象、PduR 管路由、Com 管信号提取/写入。

---

## 阶段 5：SPI 通信（2~3 周）

> **目标**: MCU Master → FT2232H Slave，32B 固定帧，差分协议，CRC8 校验。

### SPI 帧格式

```
CMD(1B) | SIZE(1B) | PAYLOAD(28B) | CRC8(1B)
```

| 命令 | 值 | 说明 |
|------|----|------|
| CMD_DIFF_POLL | 0xA2 | ★ 差分传输（推荐） |
| CMD_FULL_SYNC | 0xA3 | 强制全量同步 |
| CMD_SENSOR_POLL | 0xA0 | 全量信号 |
| CMD_HEARTBEAT | 0xA1 | 心跳 |

### 差分规则

`sensor_mask`（u32）位映射：bit=1 表示该字段有变化，按位序依次打包。
接收端维护全量状态缓存，差分帧仅更新变化字段。默认差分 + 5s 全量 + CRC 异常主动同步。

### 验证方法

```
S32K144 (Master)                 Ubuntu (Slave)
  SPI 发 32B 帧 →  FT2232H  →  libmpsse 读取
                                   │
                               SpiGateway 解码
                                   │
                               全量状态缓存
```

✅ **完成标准**: MCU 端 SPI 发送稳定（1MHz），SOC 端能正确解码差分帧，全量状态缓存一致。

**🔄 AUTOSAR CP 概念穿插**: Com Stack 负责信号↔Pdu 编解码，Spi 驱动提供 `Spi_WriteIb()` 接口。

---

## 阶段 6：SOME/IP 服务通信（3~4 周）

> **目标**: SOC 端 vsomeip 服务发布 + 端到端验证。

### SOM/IP Event 定义

| Event ID | 信号 | 类型 | Event ID | 信号 | 类型 |
|----------|------|------|----------|------|------|
| 0x8001 | 车门状态 | u8 | 0x8005 | 喇叭 | u8 |
| 0x8002 | 档位 | u8 | 0x8006 | 方向盘角度 | i16 |
| 0x8003 | 大灯 | u8 | 0x8007 | 车速 | u16 |
| 0x8004 | 转向灯 | u8 | | | |

### 端到端数据流

```
按键→MCAL ISR→RTE→SWC→SpiIf/CanIf→SPI→Platform→SpiGateway→SignalFusion→SOME/IP
```

✅ **完成标准**: 按键→SPI→vsomeip Event 订阅成功，端到端延迟 ≤ 50ms。

**🔄 AUTOSAR AP 概念穿插**: ara::com 服务模型（Method/Event/Field）、ara::sd 服务发现。

---

## 阶段 7：UDS 诊断（后续）

| 关键服务 | 预留位置 |
|---------|---------|
| 0x10 会话控制 | `soc/diag/ara/diag/` |
| 0x22 读 DID | `soc/config/domain_config.yaml` |
| 0x19 读取 DTC | （待实现） |

---

## 总结

| 阶段 | 内容 | 时间 | 产出 |
|------|------|------|------|
| 0 | 环境搭建 + 硬件确认 | 1 天 | SPI/CAN/UART 物理连通 |
| 1 | MCAL 外设驱动 | 2~3 周 | Gpio / Mcu / Adc / Can / Spi / Port |
| 2 | ECU 抽象层 + CDD | 1~2 周 | CanIf / IoHwAb / SpiIf / Uart |
| 3 | RTE + SWC | 1~2 周 | 共享缓冲区 + 周期调度 |
| 4 | CAN 通信 | 2~3 周 | FlexCAN 收发 + CAN 分析仪验证 |
| 5 | SPI 通信 | 2~3 周 | 32B 固定帧 + 差分 + CRC8 |
| 6 | SOME/IP 服务通信 | 3~4 周 | vsomeip 服务发布 + SD |
| 7 | UDS 诊断 | 后续 | 诊断协议栈 |

---

> **相关文档**
> - 架构设计: [VehicleGateway_Design.md](VehicleGateway_Design.md)
> - AUTOSAR 概念: [AUTOSAR_学习路线图.md](AUTOSAR_学习路线图.md)
> - SDK API: [S32K144_DRV_层开发指南.md](S32K144_DRV_层开发指南.md)
> - 编译烧录: [MCU_交叉编译与烧录指南.md](MCU_交叉编译与烧录指南.md)
> - CAN 教程: [从零学CAN.md](从零学CAN.md)
> - 任务跟踪: [TaskPlan.md](TaskPlan.md)
