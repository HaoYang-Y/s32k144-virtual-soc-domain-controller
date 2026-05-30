# AUTOSAR 概念学习路线图

> **学习策略**: 不单独学 AUTOSAR，而是**随 CAN / SOME/IP 学习过程穿插理解**。
> 学一层通信，就附带上层 AUTOSAR 抽象概念，在实践中建立认知。

---

## 第一阶段：MCU CAN 通信基础（无 AUTOSAR）

> **你已有 Linux 开发基础，MCU 端 MCAL 层直接操作寄存器（学习目的）。**
> 此阶段只关注 CAN 通信必须的最小知识集：C 嵌入式基础 + 时钟系统 + FlexCAN。
> 其他外设（GPIO/SPI/TIMER/ADC）用到时再学。

---

## 第二阶段：CAN 通信精进（附 AUTOSAR CP 概念）

### 2.1 FlexCAN SDK 驱动 → MCAL Can

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `flexcan_init()` | **Can_Init()** — MCAL Can 驱动 | 初始化 CAN 控制器、波特率、掩码 |
| `flexcan_send_msg()` | **Can_Write()** — MCAL Can 驱动 | 构造并发送 CAN 帧 |
| `flexcan_recv_msg()` | **Can_Read()** / **Can_RxIndication()** | 接收中断/查询读取 |
| MB 缓冲区管理 | **CanHardwareObject** | 硬件对象对应消息缓冲区 |

**一句话**: AUTOSAR CP 的 MCAL Can 层就是对 FlexCAN 驱动的标准化封装。

### 2.2 SocketCAN SOC 端 → CanIf

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `CanBus::send()` | **CanIf_Transmit()** | 统一 CAN 帧发送接口 |
| `CanBus::recv()` | **CanIf_RxIndication()** | 统一 CAN 帧接收回调 |
| 接口抽象 (`can0`) | **CanIf** 层 | 屏蔽具体 CAN 硬件差异 |

**一句话**: CanIf 层让上层网络协议（如 DoIP、J1939）不关心底层是哪个 CAN 控制器。

### 2.3 SPI 通信 → Com Stack + Spi

| 你的代码 | AUTOSAR CP 概念 | 核心关系 |
|---------|----------------|---------|
| `ComStack_ProcessSpi()` | **PduR** + **Com** | 差分编码 + 信号→PDU 编解码 |
| `Spi_WriteIb()` / `Spi_ReadIb()` | **Spi** (MCAL) | SPI 32B 固定帧收发 |
| CRC8 校验 / 差分 mask | **E2E_P01** (CP) | 数据完整性保护 |

**一句话**: MCU 端 Com Stack 通过 SPI 发送差分帧给 SOC，同时 FlexCAN 发送 CAN 帧给其他域控制器——SPI 和 CAN 是 MCU 端两个并行的输出通道。

### 2.4 CAN 网络管理 → CanNm

| 概念 | AUTOSAR CP CanNm | 作用 |
|------|------------------|------|
| NM 报文 (0x7FE) | **CanNm** | 所有节点周期性发送 NM 帧宣告在线 |
| 网络状态机 | Repeat → Normal → BusSleep | 协调节点集体睡眠/唤醒 |
| NM 协调算法 | **CanNm_NetworkRequest()** | 某个节点请求网络保持唤醒 |

**一句话**: CanNm 保证"一个节点还在工作，所有节点都别睡"。

---

## 第三阶段：SOME/IP 服务通信（附 AUTOSAR AP 概念）

### 3.1 SOME/IP 协议 → ara::com

| SOME/IP 概念 | AUTOSAR AP 概念 | 核心关系 |
|-------------|----------------|---------|
| Method (请求/响应) | **ara::com Method** | 客户端调用，返回结果 |
| Event (事件推送) | **ara::com Event** | 服务端主动通知客户端 |
| Field (属性) | **ara::com Field** | 可读可写的服务属性，带通知 |
| Service ID + Method ID | **ServiceInterface** | 唯一标识一个服务方法 |

**一句话**: ara::com 是 AUTOSAR AP 对 SOME/IP 的 C++ 封装——SOME/IP 是"网络协议"，ara::com 是"编程模型"。

### 3.2 SOME/IP 服务发现 → ara::sd

| SOME/IP SD 概念 | AUTOSAR AP 概念 | 核心关系 |
|----------------|----------------|---------|
| OfferService | **ara::sd::OfferService** | 服务端宣布自己启动 |
| FindService | **ara::sd::FindService** | 客户端发现可用服务 |
| SubscribeEventgroup | **ara::sd::Subscribe** | 客户端订阅事件组 |

**一句话**: ara::sd 让服务之间的发现和绑定零配置、动态化，是实现 SOA 的核心。

### 3.3 SOME/IP 序列化 → E2E (端到端保护)

| SOME/IP 序列化 | AUTOSAR AP E2E | 核心关系 |
|---------------|---------------|---------|
| 序列化规则 | **E2E_P01/P02** | 自定义数据序列化 + CRC |
| 反序列化 | **E2E_Check()** | 校验数据完整性和真实性 |

**一句话**: E2E 在 SOME/IP 序列化基础上增加了 CRC 和计数器，防止通信篡改和重放。

---

## 整体概念地图

```
  ┌────────────────────────────────────────────────────────┐
  │               AUTOSAR AP (自适应平台)                    │
  │  ┌──────────┬───────────┬─────────────┬─────────────┐  │
  │  │ ara::com │ ara::sd   │  ara::os    │  ara::diag  │  │
  │  │ 服务模型  │ 服务发现  │  操作系统    │  诊断管理   │  │
  │  └────┬─────┴────┬──────┴──────┬──────┴──────┬──────┘  │
  │       │          │             │             │         │
  │       └── SOME/IP 协议 ────── SOME/IP SD ──── UDS ──┘  │
  ├────────────────────────────────────────────────────────┤
  │               AUTOSAR CP (经典平台)                     │
  │  ┌─────────┬────────┬────────┬─────────┬─────────┐    │
  │  │  Com    │ PduR   │ CanIf  │  CanNm  │  Can    │    │
  │  │ 信号管理 │ PDU路由│抽象接口│网络管理   │MCAL驱动│    │
  │  └────┬────┴───┬────┴────┬───┴────┬────┴────┬────┘    │
  │       │        │         │        │         │         │
  │       └────────┴─── CAN 帧 ───────┴─────────┘         │
  ├────────────────────────────────────────────────────────┤
  │              硬件层 (当前动手区)                        │
  │   S32K144 FlexCAN ─── USB-CAN 工具 ─── SocketCAN      │
  │   (物理 MCU)          (USB 桥接)        (Ubuntu 虚拟机) │
  │                                                       │
  │   S32K144 LPSPI ──── FT2232H (SPI Slave) ── Ubuntu     │
  │   (SPI Master)       (USB-SPI 桥)          (libmpsse)  │
  └────────────────────────────────────────────────────────┘
```

---

## 学习建议

1. **不跳步**: 学 CAN 时不要急着看 ara::com，理解 CAN 帧格式后，CP 的概念自然能理解
2. **每层都动手**: FlexCAN 驱动 → SocketCAN → CAN 信号解析，每层都写代码
3. **回头看**: 学完一层后，回头看 AUTOSAR 概念对照表，你会突然明白那些抽象概念的意义
4. **概念到概念**: AUTOSAR 概念看一两遍记不住很正常，每次学新内容时温习旧概念，逐渐内化
