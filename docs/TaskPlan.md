# Domain Controller 任务计划

> 基于 `VehicleGateway_Design.md` 设计文档，按架构分层逐步实施
> 当前状态: Week 1 进行中（MCAL 层改造）

---

## 任务总览

| 阶段 | 目标 | 核心产出 | AUTOSAR 概念 |
|------|------|---------|-------------|
| Week 1 | MCU MCAL 层搭建 | GpioDrv + McuDrv + AdcDrv + PwmDrv | CP MCAL, 寄存器操作 |
| Week 2 | MCU 信号采集验证 | GPIO 按键 + ADC 旋钮 + PIT 定时采样 | CP SWC, ISR 隔离 |
| Week 3 | MCU CAN 通信 | FlexCAN 收发 + CAN 分析仪验证 | CP Com, PduR, CanIf |
| Week 4 | MCU↔MPU SPI 通信 | SPI 数据帧收发 + 差分协议 | CP Spi, AP Platform |
| Week 5 | 分层架构重构 | CP RTE/SWC 分层 + AP Service 层 | CP RTE, AP Service |
| Week 6 | SOME/IP 全链路 | vsomeip 服务发布 + 端到端验证 | AP ara::com, SD |

---

## Week 1: MCU MCAL 层搭建

> 目标: 按 AUTOSAR CP MCAL 规范组织 MCU 驱动代码，全部直接操作寄存器

### 子任务

- [x] 创建 `mcu/mcal/` 目录
- [x] 实现 `GpioDrv.h/c`（Gpio_ReadPin, Gpio_WritePin, GpioDrv_Init）
- [x] 实现 `McuDrv.h/c`（Mcu_InitClock, Mcu_GetCoreFreq）
- [x] 更新 `mcu/Makefile` 适配新目录结构
- [x] 编译验证通过
- [ ] 实现 `AdcDrv.h/c`（Adc_ReadGroup, AdcDrv_Init）
- [ ] 实现 `PwmDrv.h/c`（Pwm_SetPeriod — 用于 PIT 定时器）
- [ ] `main.c` 集成测试（打印按键状态 + ADC 值到串口）
- [ ] 烧录到 S32K144 开发板验证

### 产出文件

```
mcu/mcal/
├── GpioDrv.h    ✅
├── GpioDrv.c    ✅
├── McuDrv.h     ✅
├── McuDrv.c     ✅
├── AdcDrv.h     □
├── AdcDrv.c     □
├── PwmDrv.h     □
└── PwmDrv.c     □
```

---

## Week 2: MCU 信号采集验证

> 目标: GPIO 按键 + ADC 旋钮 + PIT 定时采样，中断写 → 主循环读

- [ ] GPIO 中断配置（按键事件驱动）
- [ ] ADC 通道配置 + PIT 50ms 定时采样
- [ ] `g_shared_signals` 共享缓冲区（volatile struct）
- [ ] 按键状态变化 → 串口打印
- [ ] ADC 采样值 → 串口打印
- [ ] 烧录验证

---

## Week 3: MCU CAN 通信

> 目标: FlexCAN 收发 CAN 帧，USB-CAN 分析仪捕获验证

- [ ] 创建 `mcu/mcal/CanDrv.h/c`（Can_Transmit, Can_Receive）
- [ ] CAN 帧编解码（按 DBC 信号矩阵）
- [ ] 按键/ADC → 编码 CAN 帧 → FlexCAN 发送
- [ ] USB-CAN 分析仪 + `candump` 验证接收
- [ ] 双向验证: USB-CAN 发送 → S32K144 接收 → UART 打印

---

## Week 4: MCU↔MPU SPI 通信

> 目标: SPI 32B 固定帧收发，差分协议，双芯通信打通

- [ ] 创建 `mcu/mcal/SpiDrv.h/c`（Spi_WriteIb, Spi_ReadIb）
- [ ] SPI 帧格式: CMD(1B) | SIZE(1B) | PAYLOAD(28B) | CRC8(1B)
- [ ] 差分编码: sensor_mask 位映射
- [ ] CRC8-ATM 校验
- [ ] MPU 端 `platform/SpiDevice.cpp`（libmpsse 封装）
- [ ] SPI 帧收发联调验证

---

## Week 5: 分层架构重构

> 目标: 重新组织代码为 CP 三层（SWC → Com Stack → MCAL）+ AP 三层

### MCU 端（AUTOSAR CP）

- [ ] 创建 `signal_com/com_stub.h`（RTE 宏，volatile 共享内存）
- [ ] 创建 `signal_com/ComStack.h/c`（信号路由 + Pdu 编解码 + 差分编码）
- [ ] 创建 `signal_app/VehicleSpeed_SWC.c` 等 SWC 组件
- [ ] 重构 `main.c` 为 SWC 周期调度

### MPU 端（AUTOSAR AP）

- [ ] 创建 `mpu/platform/SpiDevice.cpp/h`
- [ ] 创建 `mpu/svc/SignalBridgeService.cpp/h`（SignalBuffer + DiffCodec）
- [ ] 创建 `mpu/comm/CommunicationService.cpp`（Stubs）

---

## Week 6: SOME/IP 全链路

> 目标: vsomeip 服务发布 + 端到端数据流验证

- [ ] vsomeip 环境搭建（Ubuntu 24.04）
- [ ] SOME/IP Event 发布（0x8001~0x8007）
- [ ] SignalBuffer 无锁环形缓冲区
- [ ] DiffCodec 差分帧解析 + 全量状态缓存
- [ ] 端到端验证: 按键→SPI→vsomeip Event 订阅成功

---

## 参考文档

| 文档 | 用途 |
|------|------|
| `VehicleGateway_Design.md` | 架构设计参照（信号矩阵、协议、分层） |
| `AUTOSAR_学习路线图.md` | AUTOSAR CP/AP 概念对照 |
| `从零学CAN.md` | CAN 协议 + FlexCAN SDK 教程 |
| `S32K144_DRV_层开发指南.md` | NXP SDK DRV 层 API 参考 |
| `MCU_交叉编译与烧录指南.md` | 编译工具链 + J-Link 烧录流程 |
| `usbcan_device_info.md` | USB-CAN 设备信息 |

---

> 最后更新: 2026-05-30
