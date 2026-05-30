# 🎯 学习计划 — 车载信号网关（基于 S32K144 + Ubuntu SOC）

> **目标人群**: 有 Linux C++ 基础，嵌入式 C 经验较少
> **预计时长**: 8~12 周（每周 5~8 小时）
> **核心产出**: GPIO/ADC 信号采集 → MCU CP 三层处理 → SPI 差分帧 → SOC AP 三层处理 → SOME/IP + CAN 发布
> **原则**: 先独立学 MCAL 外设驱动，再组合实现完整网关

---

## 🧩 SDK 说明

本工程 MCU 代码基于 **NXP S32 SDK for S32K1xx (RTM 4.0.2)**。当前 MCAL 层用**直接寄存器操作**（学习目的），后续 Com Stack / SWC 层逐步引入 SDK DRV API。

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
    ├── GPIO 按键 ×9（车门×4 / 档位×4 / 喇叭）
    └── ADC 旋钮 ×2（方向盘角度 / 车速）
```

> MCU 通过 SPI 将信号帧传输给 SOC，同时通过 CAN 发布到其他域控制器。
> SPI 使用 32B 固定帧 + 差分协议，详见 [VehicleGateway_Design.md](VehicleGateway_Design.md)。

---

## 学习路线总览

```
阶段 0：环境搭建与硬件拓扑                     ← 让 SPI/CAN 物理连通
        ↓
阶段 1：MCU MCAL 层搭建（GPIO→ADC→PWM→Mcu）  ← 直接寄存器操作
        ↓
阶段 2：MCU 信号采集验证（按键中断 + ADC 采样）
        ↓
阶段 3：CAN 通信精进（FlexCAN + DBC 编解码）
        ↓
阶段 4：SPI 通信（32B 固定帧 + 差分 + CRC8）
        ↓
阶段 5：SOME/IP 服务通信（vsomeip + SD）
        ↓
阶段 6：UDS 诊断（后续扩展）
```

---

## 阶段 0：环境搭建与硬件拓扑（1 天）

### 硬件确认

- 确认 S32K144 开发板供电正常
- 确认 FT2232H USB-SPI 桥被 Ubuntu 虚拟机识别（USB 直通）
- 确认 USB-CAN 工具被 Ubuntu 虚拟机识别
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
sudo apt install gcc-arm-none-eabi make
arm-none-eabi-gcc --version
```

---

## 阶段 1：MCU MCAL 层搭建（2~3 周）

> **目标**: 按 AUTOSAR CP MCAL 规范实现外设驱动，全部直接操作寄存器。

### 子任务

| 模块 | 文件 | 核心 API |
|------|------|---------|
| McuDrv | `mcu/mcal/McuDrv.h/c` | `Mcu_InitClock()`、`Mcu_GetCoreFreq()` |
| GpioDrv | `mcu/mcal/GpioDrv.h/c` | `GpioDrv_Init()`、`Gpio_ReadPin()`、`Gpio_WritePin()` |
| AdcDrv | `mcu/mcal/AdcDrv.h/c` | `AdcDrv_Init()`、`Adc_ReadGroup()` |
| PwmDrv | `mcu/mcal/PwmDrv.h/c` | `PwmDrv_Init()`（PIT 定时器封装） |

### C 语言嵌入式必备知识点

| 知识点 | 为什么重要 | 本项目应用 |
|--------|-----------|-----------|
| **typedef / 结构体** | 封装配置参数 | `GpioDrv_ConfigType` |
| **枚举与宏定义** | 定义模式/状态 | `GPIO_LEVEL_LOW`、`ADC_CH_VEHICLE_SPEED` |
| **volatile 关键字** | 防止编译器优化寄存器读取 | MCAL 寄存器映射 |
| **函数指针** | 中断回调（阶段 2 用） | ISR 注册 |
| **链接脚本** (.ld) | 内存布局 | `s32k144_flash.ld` |

### 阶段 1 验证

```bash
cd mcu && make
# 预期：源文件编译成功，生成 mcu_demo.elf
```

✅ **完成标准**: `mcu/mcal/` 下四个驱动模块编译通过，Makefile 适配新目录结构。

**🔄 AUTOSAR CP 概念穿插**: MCAL 层是 AUTOSAR CP 的最底层，所有外设驱动通过标准接口（`Xxx_Init`、`Xxx_Read/Write`）向上层暴露。

---

## 阶段 2：MCU 信号采集验证（2~3 周）

> **目标**: GPIO 按键中断 + ADC 定时采样，数据存入共享缓冲区，主循环轮询打印。

### 子任务

- [ ] GPIO 中断配置（按键事件驱动，9 个按键）
- [ ] ADC 通道配置 + PIT 50ms/100ms 定时采样
- [ ] `g_shared_signals` 共享缓冲区（`volatile struct`）
- [ ] 按键状态变化 → 打印
- [ ] ADC 采样值 → 打印
- [ ] 烧录到 S32K144 验证

### 共享缓冲区（RTE 模拟）

```c
/* com_stub.h — 零开销共享内存 */
typedef struct {
    uint8_t  door_status;      /* 车门×4 */
    uint8_t  gear_position;    /* 档位 P/R/N/D */
    uint8_t  headlight;        /* 大灯 */
    uint8_t  turn_signal;      /* 转向灯 */
    uint8_t  horn;             /* 喇叭 */
    int16_t  steering_angle;   /* 方向盘角度 */
    uint16_t vehicle_speed;    /* 车速 */
} SharedSignalsType;

volatile SharedSignalsType g_shared_signals;
```

### 阶段 2 验证

```
MCU 端 (S32K144)
  按键按下 → GPIO 中断 → 更新 g_shared_signals
  ADC 旋钮 → PIT 50ms 定时 → 更新 g_shared_signals
  main 循环 100ms → 读取 g_shared_signals → puts() 打印
```

✅ **完成标准**: 按键 + 旋钮的物理信号能被正确采集并打印。

**🔄 AUTOSAR CP 概念穿插**: RTE（运行时环境）在 AUTOSAR CP 中负责 SWC 之间的通信。本工程用 `volatile` 共享内存模拟 RTE —— ISR 写、主循环读，零开销。

---

## 阶段 3：CAN 通信（2~3 周）

> **目标**: FlexCAN 收发 CAN 帧，DBC 信号编解码。

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
  candump 发送 →       接收 →  串口打印
```

✅ **完成标准**: S32K144 能发送/接收 CAN 帧；USB-CAN 工具 `candump` 双向验证通过。

**🔄 AUTOSAR CP 概念穿插**: Can→CanIf→PduR→Com 四层 —— Can 管硬件、CanIf 管接口抽象、PduR 管路由、Com 管信号提取/写入。

---

## 阶段 4：SPI 通信（2~3 周）

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
                               DiffCodec 解码
                                   │
                               SignalBuffer 缓存
```

✅ **完成标准**: MCU 端 SPI 发送稳定（1MHz），SOC 端能正确解码差分帧，全量状态缓存一致。

**🔄 AUTOSAR CP 概念穿插**: Com Stack 负责信号↔Pdu 编解码，SpI 驱动提供 `Spi_WriteIb()` 接口。

---

## 阶段 5：SOME/IP 服务通信（3~4 周）

> **目标**: SOC 端 vsomeip 服务发布 + 端到端验证。

### SOME/IP Event 定义

| Event ID | 信号 | 类型 | Event ID | 信号 | 类型 |
|----------|------|------|----------|------|------|
| 0x8001 | 车门状态 | u8 | 0x8005 | 喇叭 | u8 |
| 0x8002 | 档位 | u8 | 0x8006 | 方向盘角度 | i16 |
| 0x8003 | 大灯 | u8 | 0x8007 | 车速 | u16 |
| 0x8004 | 转向灯 | u8 | | | |

### 端到端数据流

```
按键→MCAL→RTE→SWC→ComStack→SPI→Platform→Service→Communication
                                           Differential Codec
                                              SignalBuffer
                                                 vsomeip
```

✅ **完成标准**: 按键→SPI→vsomeip Event 订阅成功，端到端延迟 ≤ 50ms。

**🔄 AUTOSAR AP 概念穿插**: ara::com 服务模型（Method/Event/Field）、ara::sd 服务发现。

---

## 阶段 6：UDS 诊断（后续）

| 关键服务 | 预留位置 |
|---------|---------|
| 0x10 会话控制 | `soc/src/uds_server/` |
| 0x22 读 DID | `config/domain_config.yaml` |
| 0x19 读取 DTC | （待实现） |

---

## 总结

| 阶段 | 内容 | 时间 | 产出 |
|------|------|------|------|
| 0 | 环境搭建 + 硬件拓扑 | 1 天 | SPI/CAN 物理连通 |
| 1 | MCU MCAL 层搭建 | 2~3 周 | GpioDrv / McuDrv / AdcDrv / PwmDrv |
| 2 | MCU 信号采集 | 2~3 周 | 按键中断 + ADC 采样 + 共享缓冲区 |
| 3 | CAN 通信 | 2~3 周 | FlexCAN 收发 + DBC 编解码 |
| 4 | SPI 通信 | 2~3 周 | 32B 固定帧 + 差分 + CRC8 |
| 5 | SOME/IP 服务通信 | 3~4 周 | vsomeip 服务发布 + SD |
| 6 | UDS 诊断 | 后续 | 诊断协议栈 |

---

> **相关文档**
> - 架构设计: [VehicleGateway_Design.md](VehicleGateway_Design.md)
> - AUTOSAR 概念: [AUTOSAR_学习路线图.md](AUTOSAR_学习路线图.md)
> - SDK API: [S32K144_DRV_层开发指南.md](S32K144_DRV_层开发指南.md)
> - 编译烧录: [MCU_交叉编译与烧录指南.md](MCU_交叉编译与烧录指南.md)
> - CAN 教程: [从零学CAN.md](从零学CAN.md)
> - 任务跟踪: [TaskPlan.md](TaskPlan.md)
