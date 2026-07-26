# Domain Controller 任务计划

> 基于实际项目结构，按 AUTOSAR CP/AP 分层组织。
> 当前日期: 2026-07-20
>
> **CAN 栈全链路调通** ✅ — MCAL Can → CanIf → CanTp 三层验证。
> 中断驱动 RX (Can_EnableInterrupts + Can_MainFunctionRx)，分层回调架构。
> 多路 CAN 架构 (HTH 编码 Controller+MB)，CanTp SF/FF/MF 完整流控。
> CANable (gs_usb, 1d50:606f) 500kbps 稳定抓取。

---

## 当前实际进度总览

| 层 | 模块 | 目录 | 状态 |
|----|------|------|------|
| **MCAL** | Gpio | `mcu/MCAL/Gpio/` | ✅ AUTOSAR 对齐 (Read/Write/FlipChannel, Std_ReturnType) |
| **MCAL** | Mcu | `mcu/MCAL/Mcu/` | ✅ AUTOSAR 对齐 (Mcu_Init, GetResetReason, PerformReset) |
| **MCAL** | Can | `mcu/MCAL/Can/` | ✅ 多路架构 + 中断驱动 RX + HTH 编码 |
| **MCAL** | Spi | `mcu/MCAL/Spi/` | ⏳ 骨架已有 |
| **MCAL** | Port | `mcu/MCAL/Port/` | ⏳ 骨架已有 |
| **ECU Abstraction** | CanIf | `mcu/EcuAbstraction/CanIf/` | ✅ 分层回调 + PDU 翻译 + 中断 RX |
| **ECU Abstraction** | IoHwAb | `mcu/EcuAbstraction/IoHwAb/` | ⏳ 骨架已有 |
| **ECU Abstraction** | SpiIf | `mcu/EcuAbstraction/SpiIf/` | ⏳ 骨架已有 |
| **Services** | EcuM | `mcu/Services/EcuM/` | ✅ 统一 Init + MainFunction 调度 |
| **Services** | CanTp | `mcu/Services/CanTp/` | ✅ FC 流控状态机 (SF/FF/MF 已验证) |
| **Services** | PduR | `mcu/Services/PduR/` | ✅ 最小路由实现 |
| **Services** | Com | `mcu/Services/Com/` | ⏳ 骨架 |
| **CDD** | Uart | `mcu/CDD/Uart/` | ⏳ 骨架，**未调通** 🔥 |
| **RTE** | Rte | `mcu/RTE/` | ⏳ 骨架 |
| **SWC** | Swc_SignalGateway | `mcu/App/Swc_SignalGateway/` | ⏳ 骨架 |
| — | — | — | — |
| **SOC Platform** | ara/core, log, exec, Spi | `soc/platform/` | ⏳ 骨架 |
| **SOC Communication** | ara/com, SpiGateway | `soc/communication/` | ⏳ 骨架 |
| **SOC Diag** | UdsServer | `soc/diag/ara/diag/` | ⏳ 骨架 |
| **SOC App** | DomainController | `soc/apps/DomainController/` | ⏳ 骨架 |
| **Proto** | signal_gateway.proto | `proto/` | ❌ 代码生成未集成 |

---

## P0: 阻塞性基础设施 🔥

### 任务 1: CAN 收发调通

**目标**: FlexCAN0 能成功收发 CAN 帧，USB-CAN 分析仪能捕获

**涉及文件**:
- `mcu/MCAL/Can/include/Can.h` — Can_Init / Can_Transmit / Can_Receive 接口
- `mcu/MCAL/Can/src/Can.c` — FlexCAN SDK API (FLEXCAN_DRV_*) 实现
- `mcu/MCAL/Can/config/Can_Cfg.h` — CAN 波特率/MB 配置
- `mcu/EcuAbstraction/CanIf/include/CanIf.h` — CanIf_Transmit 抽象接口
- `mcu/EcuAbstraction/CanIf/src/CanIf.c` — 调用 Can_* API

**子任务**:
- [x] 确认 CAN 时钟使能 (PCC → FlexCAN0, SOSC_DIV1=8MHz)
- [x] 配置引脚复用 (Port 模块: PTE4=CAN0_RX, PTE5=CAN0_TX, ALT5)
- [x] 实现 Can_Init (500kbps, 13TQ: prop=7/ps1=4/ps2=1, pre_div=0)
- [x] 实现 Can_Write (每次发前重配 MB: CAN_ConfigTxBuff → CAN_Send)
- [x] 实现 Can_Read (轮询, CAN_Receive PAL API)
- [x] CanIf 封装 (骨架已有，上层通过 CanIf_Transmit 调用 Can_Write)
- [x] USB-CAN 分析仪 candump 验证 — CANable (gs_usb, 1d50:606f)
- [x] 烧录到 S32K144 验证

**Bug 修复记录**: 参见 [BugfixLog.md](BugfixLog.md)

**验证方法**:
```
S32K144 FlexCAN0 ──── CAN 帧 ──── USB-CAN 分析仪 ──── candump can0
  MCU 周期发送 0x123#XXXXXXXXXX... ──→  捕获 ID=0x123
  cansend can0 100#AABBCCDD ──→  MCU RX 中断 → Can_MainFunctionRx → CanIf → PduR → CanTp
```

### 任务 2: UART 日志输出

**目标**: LPUART1 能输出调试日志到宿主机串口

**涉及文件**:
- `mcu/CDD/Uart/include/Uart.h` — Uart_Init / Uart_SendString 接口
- `mcu/CDD/Uart/src/Uart.c` — LPUART SDK API (LPUART_DRV_*) 实现
- `mcu/CDD/Uart/config/Uart_Cfg.h` — 波特率/引脚配置

**子任务**:
- [ ] 确认 LPUART 时钟使能
- [ ] 配置引脚复用 (Port 模块)
- [ ] 实现 Uart_Init (115200, 8N1)
- [ ] 实现 Uart_SendString (阻塞发送)
- [ ] 宿主机串口工具验证输出
- [ ] 烧录到 S32K144 验证

---

## P1: MCAL 外设驱动验证

### 任务 3: GPIO 按键输入
**涉及**: `mcu/MCAL/Gpio/`, `mcu/MCAL/Port/`, `mcu/EcuAbstraction/IoHwAb/`
- [x] Gpio_Init / Gpio_ReadChannel / Gpio_WriteChannel / Gpio_FlipChannel (AUTOSAR 对齐)
- [ ] Port_SetPinDirection 引脚方向配置
- [ ] IoHwAb_ReadPin 抽象封装
- [ ] 按键中断 ISR → 更新共享缓冲区

### 任务 4: MCU 时钟配置
**涉及**: `mcu/MCAL/Mcu/`
- [x] Mcu_Init 实现 (替代 Mcu_InitClock, Std_ReturnType)
- [x] Mcu_GetCoreFreq 实现
- [x] Mcu_GetResetReason 实现
- [x] Mcu_PerformReset 实现
- [ ] clock_config.c SPLL 配置

### 任务 5: SPI 驱动
**涉及**: `mcu/MCAL/Spi/`
- [ ] Spi_Init (LPSPI0, Master, 1MHz)
- [ ] Spi_WriteIb / Spi_ReadIb 实现
- [ ] 32B 固定帧收发验证

---

## P2: ECU 抽象层 + RTE

### 任务 6: CanIf 接口
**涉及**: `mcu/EcuAbstraction/CanIf/`
- [x] CanIf_Transmit → Can_Write (HTH 编码, PDU→CAN ID 查表)
- [x] CanIf_RxIndication 回调机制 → CanIf_McalRxCallback + PDU ID 翻译
- [x] CanIf_Init 向 MCAL 注册 RX 回调 (Can_RegisterRxCallback)

### 任务 7: SpiIf 接口
**涉及**: `mcu/EcuAbstraction/SpiIf/`
- [ ] SpiIf_WriteIb → 调用 Spi_WriteIb

### 任务 8: RTE 运行时环境
**涉及**: `mcu/RTE/`
- [ ] Rte_Read / Rte_Write 宏
- [ ] g_shared_signals volatile 共享缓冲区

---

## P3: SWC + 全链路

### 任务 9: SWC 信号采集调度
**涉及**: `mcu/App/Swc_SignalGateway/`
- [ ] Swc_SignalGateway_Run 周期调度
- [ ] 通过 RTE 读写信号
- [ ] 差分编码 + SPI 发送
- [ ] CAN 帧编码 + 发送

### 任务 10: MCU↔SOC SPI 通信联调
**涉及**: `mcu/MCAL/Spi/` + `soc/platform/Spi/` + `soc/communication/SpiGateway/`
- [ ] SOC 端 libmpsse 初始化 SPI Slave
- [ ] SpiGateway 差分帧解码
- [ ] 全量状态缓存一致性验证

---

## P4: SOC 端服务

### 任务 11: SignalFusion 信号融合
**涉及**: `soc/apps/DomainController/`
- [ ] ConfigManager 读取 domain_config.yaml
- [ ] SignalFusion 信号处理 + 状态管理

### 任务 12: SOME/IP 服务
**涉及**: `soc/communication/ara/com/`
- [ ] vsomeip 环境搭建
- [ ] Event 发布 (0x8001~0x8007)
- [ ] Service Discovery

### 任务 13: UDS 诊断
**涉及**: `soc/diag/ara/diag/`
- [ ] UdsServer 骨架实现
- [ ] 0x10 会话控制 / 0x22 读 DID / 0x19 DTC

---

## P5: 远期

### 任务 14: Protobuf 代码生成
**涉及**: `proto/signal_gateway.proto`
- [ ] MCU: nanopb 生成 C 代码 → `mcu/proto/`
- [ ] SOC: protoc 生成 C++ 代码 → `soc/proto/`
- [ ] 集成到构建系统

### 任务 15: DBC 信号矩阵
- [ ] 编写 `tools/dbc/vehicle_signals.dbc`
- [ ] CAN 发送/接收自动按 DBC 编解码

---

## 参考文档

| 文档 | 用途 |
|------|------|
| `VehicleGateway_Design.md` | 架构设计参照（信号矩阵、协议、分层） |
| `AUTOSAR_学习路线图.md` | AUTOSAR CP/AP 概念对照 |
| `从零学CAN.md` | CAN 协议 + FlexCAN SDK 教程 |
| `S32K144_DRV_层开发指南.md` | NXP SDK DRV 层 API 参考 |
| `MCU_交叉编译与烧录指南.md` | 编译工具链 + J-Link 烧录流程 |

---

> 最后更新: 2026-07-17
