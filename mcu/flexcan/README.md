# FlexCAN 模块 — 对应书籍第 10 章

> **CAN 通信是项目两大核心主线之一**
>
> 本章是 **汽车通信入门第一站**：从 FlexCAN 寄存器驱动到 AUTOSAR CP Can 模块概念。
> 学完后进入第二阶段 SocketCAN SOC 端学习。

## 学习目标

- [ ] 理解 CAN 总线协议（帧格式、仲裁、位填充）
- [ ] 掌握 FlexCAN 模块寄存器结构（MCR/CTRL1/MBn/IFLAG1）
- [ ] 实现 CAN 初始化和波特率配置
- [ ] 实现 CAN 消息发送和接收
- [ ] 理解消息缓冲区 (MB) 的工作机制
- [ ] **理解 AUTOSAR CP MCAL Can 模块与寄存器驱动的映射关系**

## 书籍对应页码

| 小节 | 内容 | 页数 |
|------|------|------|
| §10.1 | CAN 总线基础 | p267~280 |
| §10.2 | CAN 编程结构 | p280~300 |
| §10.3 | CAN 驱动构件封装 | p300~320 |
| §10.4 | CAN 应用实验 | p320~330 |

## 要实现的函数清单

在 `include/flexcan_driver.h` 和 `src/flexcan_driver.c` 中实现：

1. `flexcan_init()`   — 初始化 FlexCAN 模块（时钟使能、波特率、掩码）
2. `flexcan_send_msg()` — 发送 CAN 数据帧
3. `flexcan_recv_msg()` — 接收 CAN 帧（查询/中断）

## 硬件连接

```
S32K144 开发板                    USB-CAN 工具              Ubuntu 虚拟机
┌─────────────────────┐         ┌──────────────┐         ┌─────────────────┐
│ CAN0_TX (PTD1) ─────┼───┬───→│              │         │                 │
│ CAN0_RX (PTD0) ─────┼───┘   │ CAN H ←───────┤USB─────│ can0            │
│                     │        │ CAN L ←───────┤        │ sudo ip link    │
│ CAN 收发器使能 ─────┼───→    │              │         │   set can0 up   │
│                     │        │ 120Ω 终端电阻 │         │ candump can0    │
│ 供电: 5V / VBUS ───┼───→    │ (H↔L 间跨接)  │         │                 │
└─────────────────────┘        └──────────────┘         └─────────────────┘
```

**连接步骤**:
1. 确认开发板 CAN 收发器供电正常（通常由开发板 5V 供电）
2. CAN H 接 USB-CAN 工具 CAN H，CAN L 接 USB-CAN 工具 CAN L
3. USB-CAN 工具两端 H/L 之间接 **120Ω 终端电阻**（如果开发板已内置则跳过）
4. USB-CAN 工具插入电脑 USB 口，VMware 中设为 **USB 直通**
5. 虚拟机内加载驱动并启动 can0（配置同波特率）

**引脚排查**（确认你的开发板使用哪个 CAN 引脚）:
```
CAN0_TX 可能引脚: PTD1 / PTE4 / PTA12   (查开发板原理图)
CAN0_RX 可能引脚: PTD0 / PTE5 / PTA13   (查开发板原理图)
```

## 编译运行

```bash
cd mcu/flexcan
make all        # 编译
make flash      # 烧录
```

## 验证标准

- [ ] `make all` 编译无报错
- [ ] USB-CAN 工具被 Ubuntu 虚拟机识别（`dmesg` 确认）
- [ ] 虚拟机内 `sudo ip link set can0 up type can bitrate 500000` 成功
- [ ] 虚拟机内 `candump can0` 能看到 S32K144 发出的 CAN 帧（如 `0x123#01020304`）
- [ ] 能解释每个配置位的作用（MCR 的 8~5bit、CTRL1 的 12~15bit 等）

---

## 🏛 附：AUTOSAR CP CAN 协议栈概念对照

> 学完本节寄存器驱动后，回头看这个表，理解寄存器驱动在 AUTOSAR 架构中的位置。

### 分层结构（自底向上）

```
┌──────────────────────────────────────────────────┐
│  Com (通信管理器)                                  │
│  ├ 信号级收发 ─── 信号变化通知、信号网关             │
│  └ 对应本项目: SignalFusion (signal_manager)       │
├──────────────────────────────────────────────────┤
│  PduR (PDU 路由器)                                │
│  ├ PDU 级路由 ─── CAN↔CAN / CAN↔UDP 路由           │
│  └ 对应本项目: CanBus 上层路由逻辑                  │
├──────────────────────────────────────────────────┤
│  CanIf (CAN 接口层)                                │
│  ├ 抽象 CAN 硬件，统一收发接口                       │
│  ├ CanIf_Transmit / CanIf_RxIndication            │
│  └ 对应本项目: CanBus (can_manager)                │
├──────────────────────────────────────────────────┤
│  MCAL: Can 驱动  ← ⬅ 你现在正在学这一层             │
│  ├ Can_Init / Can_Write / Can_Read                │
│  └ 对应本项目: flexcan_driver.c                      │
├──────────────────────────────────────────────────┤
│  硬件: S32K144 FlexCAN 模块                        │
│  └ MB 缓冲区、CAN 收发器                             │
└──────────────────────────────────────────────────┘
```

### 关键函数映射

| 本项目函数 | AUTOSAR CP MCAL Can 函数 | 功能 |
|-----------|-------------------------|------|
| `flexcan_init()` | `Can_Init()` | 初始化 CAN 控制器、配置波特率 |
| `flexcan_send_msg()` | `Can_Write()` | 发送 CAN 帧 |
| `flexcan_recv_msg()` | `Can_Read()` / `Can_RxIndication()` | 接收 CAN 帧 |
| (中断回调) | `Can_ControllerBusOff()` | 总线关闭处理 |
| (待实现) | `Can_SetBaudrate()` | 动态波特率配置 |

### 学习路径：从寄存器到 AUTOSAR CP

1. **第一步**（当前）：实现 FlexCAN 寄存器驱动，理解 CAN 帧收发底层原理
2. **第二步**（SOC 端 CanBus）：实现 CanIf 层——封装 SocketCAN，为上层提供统一收发接口
3. **第三步**（CAN 信号解析）：实现 PduR + Com——从原始帧提取信号并融合
4. **第四步**（CAN 网络管理）：引入 CanNm——理解节点睡眠/唤醒协调

> 每步推进时回头看这个表，理解你写的代码在 AUTOSAR CP 协议栈中的位置。
