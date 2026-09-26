# Domain Controller 任务计划

> 基于实际项目结构，按 AUTOSAR CP/AP 分层组织。
> 当前日期: 2026-08-23
>
> **CAN 栈全链路打通** ✅ — MCAL Can → CanIf → CanTp → PduR → Com → RTE → SWC 全部实现并测试通过。
> CANable (gs_usb, 1d50:606f) 500kbps TX/RX 实测验证。
> **SPI 链路打通** ✅ — CH347T ↔ S32K144 LPSPI1 Slave (1MHz, Mode 0) 双向通信实测验证。

---

## 当前实际进度总览

| 层 | 模块 | 目录 | 状态 |
|----|------|------|------|
| **MCAL** | Gpio | `mcu/MCAL/Gpio/` | ✅ |
| **MCAL** | Mcu | `mcu/MCAL/Mcu/` | ✅ |
| **MCAL** | Can | `mcu/MCAL/Can/` | ✅ 多路架构 + 中断驱动 + HTH 编码 + TX 回调 |
| **MCAL** | Spi | `mcu/MCAL/Spi/` | ✅ LPSPI1 Slave 模式，链路实测打通 |
| **MCAL** | Port | `mcu/MCAL/Port/` | ✅ |
| **ECU Abstraction** | CanIf | `mcu/EcuAbstraction/CanIf/` | ✅ 分层回调 + PDU 翻译 + L-PDU 转换 |
| **ECU Abstraction** | SpiIf | `mcu/EcuAbstraction/SpiIf/` | ⏳ 骨架已有 |
| **Services** | EcuM | `mcu/Services/EcuM/` | ✅ 统一 Init + MainFunction 全栈调度 |
| **Services** | CanTp | `mcu/Services/CanTp/` | ✅ SF/FF/MF + FC 流控 + PCI 编解码 |
| **Services** | PduR | `mcu/Services/PduR/` | ✅ Com↔CanTp 双向路由 + TX 确认链 + 直传路径 |
| **Services** | Com | `mcu/Services/Com/` | ✅ 信号编解码 + Update Bit + Deadline + DET |
| **Services** | Det | `mcu/Services/Det/` | ✅ 开发错误追踪 |
| **Services** | BswM | `mcu/Services/BswM/` | ⏳ 骨架 |
| **CDD** | Uart | `mcu/CDD/Uart/` | ✅ 代码实现，**硬件 USB-UART 未验证** |
| **RTE** | Rte | `mcu/RTE/` | ✅ Rte_Write→Com_SendSignal, Rte_Read→Com_ReceiveSignal |
| **SWC** | Swc_SignalGateway | `mcu/App/Swc_SignalGateway/` | ✅ 走完整 AUTOSAR 链路收发 CAN 帧 |
| — | — | — | — |
| **SOC Platform** | ara/core, log, exec | `soc/platform/` | ⏳ 骨架 |
| **SOC Platform** | Spi 平台驱动 | `soc/platform/Spi/` | ⏳ SpiDriver 骨架；CH347T 内核驱动 + spidev_test 已通 |
| **SOC Communication** | ara/com | `soc/communication/` | ⏳ 骨架 |
| **SOC Communication** | SpiGateway | `soc/communication/SpiGateway/` | ⏳ 骨架（联调暂经 spidev_test 直通） |
| **SOC Diag** | UdsServer | `soc/diag/ara/diag/` | ❌ 不做 |
| **SOC App** | DomainController | `soc/apps/DomainController/` | ⏳ 骨架 |
| **Proto** | signal_gateway.proto | `proto/` | ❌ 代码生成未集成 |

---

## 待做任务

### MCAL

- [ ] **IoHwAb**: IoHwAb_ReadPin 抽象封装
- [ ] **UART 硬件验证**: 宿主机串口工具接收 UART 日志

### ECU 抽象层 + Services

- [ ] **SpiIf 接口**: SpiIf_WriteIb/ReadIb → 调用 MCAL Spi 驱动，替换骨架
- [ ] **BswM**: BSW 模式管理器 (模式仲裁、通信启停、ECU 状态切换)

### SOC 端

- [ ] **SpiDriver + SpiGateway 实现**: 基于 CH347T 已验证链路，用 SpiDriver/SpiGateway 替换 spidev_test 直通，走正式网关路径联调
- [ ] **SignalFusion**: ConfigManager + 信号处理
- [ ] **SOME/IP 服务**: vsomeip Event 发布 + Service Discovery

### 远期

- [ ] **Protobuf 代码生成**: nanopb (MCU) + protoc (SOC)
- [ ] **DBC 信号矩阵**: CAN 信号编解码标准化
- [ ] **CanTp TX 多帧测试**: CANable 刷 candleLight 固件 (带 FC 自动回复)

---

> 最后更新: 2026-08-23 (SPI 链路实测打通；UDS 决定不做)
