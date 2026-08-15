# CH347T ↔ S32K144 SPI 链路打通手顺

> 最后更新: 2026-08-15
> 状态: ✅ 链路已实测打通 (SOC↔MCU 双向通信验证通过)

## 1. 概述

本文档记录 CH347T USB-SPI 桥与 S32K144 开发板之间 SPI 通信的完整接线与测试手顺。

**架构拓扑：**

```
SOC (Ubuntu 虚拟机)  ←USB→  CH347T (SPI Master)  ←4线SPI→  S32K144 (SPI Slave)
```

**关键事实：** CH347T 这类 USB-SPI 桥芯片**只能做 Master**，因此 S32K144 必须配置为
SPI Slave (LPSPI1, PCS3)。

**通信速率：** 1 MHz，SPI Mode 0 (CPOL=0, CPHA=0)，MSB first，8 bit/frame。

---

## 2. 硬件准备

| 物料 | 说明 |
|------|------|
| CH347T 模块 | 成品模块 (免焊接)，Type-C 线连 SOC |
| S32K144-EVB | 已烧录 SPI Slave 固件 (见 §6) |
| 杜邦线 ×6 | 母对母，其中至少 5 根用于 SPI + GND |
| J-Link | 烧录 MCU 固件 |

**CH347T 模块拨码开关：** 必须拨到 **MODE 1**（S1=ON, S2=OFF）。
MODE 1 = UART1 + I2C + SPI (VCP)。拨错模式（如 MODE 0 双串口）时 CS 引脚会被
UART 功能占用，表现为数据线正常但 CS 永不动作。
拨码后需**重新插拔 USB** 让芯片重新枚举。

---

## 3. 接线手顺

### 3.1 引脚映射

| CH347T 引脚 | S32K144 引脚 | 芯片功能 | 说明 |
|-------------|-------------|---------|------|
| SCK | PTB14 | LPSPI1_SCK | 时钟 |
| MO (MOSI) | **PTB15** | LPSPI1_SIN | 数据进 MCU |
| MI (MISO) | **PTB16** | LPSPI1_SOUT | 数据出 MCU |
| CS0 | PTB17 | LPSPI1_PCS3 | 片选 (CS1/CS2 悬空) |
| GND | GND | — | **必须共地** |
| VCC | **不接** | — | 双方各自 USB 供电 |

> ⚠️ **最容易踩的坑：数据线方向。**
> 板子丝印的 "MOSI/MISO" 是 **MCU 当主机**视角的命名。MCU 当**从机**时方向反转：
>
> ```
> MO (主机输出) → PTB15 (丝印写 MISO, 芯片功能是 SIN=输入)   ← 进 MCU
> MI (主机输入) ← PTB16 (丝印写 MOSI, 芯片功能是 SOUT=输出)  ← 出 MCU
> ```
>
> 规律：**MO 接丝印 "MISO" 脚，MI 接丝印 "MOSI" 脚**。接反的症状是
> SOC 读到全 0xFF（两根数据线一头悬空没人驱动）。

### 3.2 自检清单

- [ ] CH347T 拨码 = MODE 1 (S1=ON, S2=OFF)
- [ ] MO → PTB15（丝印 MISO），MI ← PTB16（丝印 MOSI）
- [ ] CS0 → PTB17，VCC 悬空，GND 共地
- [ ] S32K144 已上电（J-Link 显示 VTref=3.3V）

---

## 4. SOC 端驱动部署

### 4.1 编译加载驱动

驱动源码位于 `soc/platform/Spi/ch347_vcp/`，包含 CS 修复（上游 set_cs 条件
写反 + CS-to-clock 5ms 延时）：

```bash
cd soc/platform/Spi/ch347_vcp
make                                    # 生成 mfd-ch347.ko / spi-ch347.ko
sudo insmod mfd-ch347.ko                # 先 MFD
sudo insmod spi-ch347.ko                # 再 SPI
sudo modprobe spidev
ls /sys/class/spi_master/               # 应出现 spi0
```

> 注：`gpio-ch347.ko` 已适配新内核 gpio_chip API（void→int 返回），需要时可一并加载。

### 4.2 创建 spidev 设备

每次**重新插拔 CH347T** 后都要重做（USB 重新枚举会清掉设备）：

```bash
sudo sh -c 'echo "spidev 0 8 0 1000000" > /sys/class/spi_master/spi0/new_device'
sudo sh -c 'echo spidev > /sys/bus/spi/devices/spi0.0/driver_override'
sudo sh -c 'echo spi0.0 > /sys/bus/spi/drivers/spidev/bind'
ls /dev/spidev0.0                       # 应出现
```

参数格式：`{驱动} {mode} {bits_per_word} {cs} {speed_hz}`。

### 4.3 自环测试（验证 CH347T 本身）

不接 MCU，把 CH347T 的 **MO 和 MI 短接**：

```bash
gcc -o spidev_test soc/platform/Spi/spidev_test.c
sudo ./spidev_test /dev/spidev0.0
```

预期 `PASS: TX == RX`。这验证 SCK/MOSI/MISO 三线正常（注意：**不验证 CS**）。

---

## 5. 联调测试

### 5.1 烧录 MCU 固件

```bash
make -C mcu clean && make -C mcu
cd mcu && JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1 -NoGui 1 -CommandFile flash.jlink
```

### 5.2 运行测试

MCU 复位后依次执行 LED 诊断 → 60 秒等待 SPI 传输：

```bash
# 复位 MCU
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1 -NoGui 1 <<'CMD'
r
g
q
CMD

sleep 3     # 等 MCU 完成 LED 诊断进入 SPI 等待
sudo ./spidev_test /dev/spidev0.0
```

### 5.3 预期结果

```
Recv (64 bytes): FF A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE ...
```

- `A0 A1 A2...` = MCU 预填的 TX 递增模式 → **MCU→SOC 方向通**
- MCU 同时收到 SOC 发的 `00 01 02...` → **SOC→MCU 方向通**
- `spidev_test` 报 FAIL 是正常的（它按自环标准比对 TX==RX，MCU 通信本来就不同）
- **首字节 FF**：SPI 从机经典现象（首字节在 FIFO 就绪前被时钟移出）。
  正式协议设计时用首字节做同步/命令字即可

### 5.4 MCU LED 状态

| 阶段 | LED |
|------|-----|
| 蓝灯闪 1 次 | Spi_SlaveInit 成功 |
| 绿灯闪 3 次 | 进入 SPI 等待 |
| 绿灯常亮 | SPI 收发成功 |
| 橙灯常亮 | SPI 超时 |

---

## 6. MCU 侧实现要点

- **引脚复用**：PTB14/15/16/17 全部 ALT3（`pin_mux.c`）
- **外设**：LPSPI1 Slave，PCS3（`Spi.c` → `Spi_SlaveInit(3U)`）
- **驱动模式**：纯轮询（忙等读 RDF/TDF 标志），不依赖 SDK ISR
- **时钟**：LPSPI1_CLK = FIRC/2 = 24MHz（`clock_config.c` 已启用）
- **SDK 依赖**：`lpspi_slave_driver.c` + `lpspi_shared_function.c` +
  `lpspi_hw_access.c` + `lpspi_irq.c`（Makefile 已添加）

---

## 7. 故障排查

| 症状 | 原因 | 解法 |
|------|------|------|
| 自环 PASS 但连 MCU 全 0xFF | **MO/MI 接反**（最常见） | MO→丝印"MISO"脚, MI←丝印"MOSI"脚 |
| 数据线对但全 0xFF | 驱动 set_cs 条件 bug | 确认 `set_cs` 中 `if (spi->mode & SPI_NO_CS) return;` |
| CS 有动作 MCU 仍不收 | CS 与首个时钟间隔太短 | 驱动 `set_cs(true)` 后有 `msleep(5)` |
| 拔插 USB 后 open 失败 | spidev 设备消失 | 重做 §4.2（模块与驱动需重新加载） |
| MCU 无响应且红灯 | MCU 还没进入等待态 | 复位后多等 3 秒再发包 |
| 拨码 MODE 0 时 CS 失效 | CS 脚被 UART1 占用 | 拨到 MODE 1 并重插 USB |

---

## 8. 历史记录

- **2026-08-15** — SPI 链路首次打通 ✅
  - 排查过程三大修复：①驱动 set_cs 的 SPI_NO_CS 条件写反；②CS-to-clock 无延时；
    ③数据线 MO/MI 接反（丝印主机视角陷阱）
  - 验证方法：CS0↔MISO 短接证明 CS 硬件有输出；PTB17 接地证明 MCU 从机可响应
