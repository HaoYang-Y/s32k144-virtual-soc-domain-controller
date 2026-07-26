# Bug 修复记录

> 记录 CAN 栈开发过程中遇到的硬件、配置、架构问题及修复方案。

---

## 任务 1: CAN 收发调通

1. **CANH/CANL 接反** → 对调
2. **CAN FD 未关闭** → `Can_BuildSdkConfig` 中显式 `fd_enable=false`
3. **LED 引脚映射错误** → PTD0=橙, PTD1=红, PTD15=绿, PTD16=蓝
4. **Mailbox 状态不释放** → 每发前调 `CAN_ConfigTxBuff` 重配 TX MB（关键修复）
5. **MCAL Can.c 改用 PAL 层实现** — 保持 AUTOSAR `Can.h` 接口不变，底层从 `FLEXCAN_DRV` 切换到 CAN PAL，解决状态管理和中断依赖问题。
6. **编译参数对齐卖家** — `-mfloat-abi=hard -mfpu=fpv4-sp-d16`，`--specs=nano.specs`，链接完整 EDMA 驱动。
7. **时钟配置** — FlexCAN0 PCC=`CLK_SRC_OFF`, `peClkSrc=CAN_CLK_SOURCE_OSC`
8. **新增 `tools/can_setup.sh`** — 一键管理 `can0` 接口。
9. **SOSC 晶振不起振** → `clock_config.c`: `SCG_SOSC_RANGE_HIGH→MID` (8MHz 必须用 MID)
10. **CAN RX 轮询→中断驱动** → `Can_EnableInterrupts` + `Can_MainFunctionRx` + 静态 buffer
11. **MCAL→CanIf 反向依赖修复** → 回调注册模式 (`Can_RxNotificationType` → `Can_RegisterRxCallback`)
12. **多路 CAN 架构** → HTH 编码 Controller+MB (`SWS_Can_00009`)，每控制器独立实例

---

> 最后更新: 2026-07-26
