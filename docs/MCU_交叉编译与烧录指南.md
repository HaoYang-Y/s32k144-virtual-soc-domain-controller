# S32K144 交叉编译与 J-Link 烧录指南

## 1. 概述

本文档说明如何将 Domain_Controller 工程中的 MCU 驱动模块（gpio/uart/timer/adc/flexcan/clock）交叉编译为 S32K144 可执行文件，并通过 J-Link 烧录到开发板。

**整体工作流：**

```
源码 (*.c)  →  arm-none-eabi-gcc 交叉编译  →  .elf / .hex / .bin  →  JLinkExe 烧录 → S32K144
```

---

## 2. 环境准备

### 2.1 交叉编译工具链

安装 `arm-none-eabi-gcc`（ARM Cortex-M 交叉编译器）：

```bash
# Ubuntu / Debian
sudo apt install gcc-arm-none-eabi

# 验证安装
arm-none-eabi-gcc --version
```

工具链基于 `cortex-m4` 内核，使用软浮点（`-mfloat-abi=soft`）。

### 2.2 J-Link 烧录软件

从 Segger 官网下载 Linux 64-bit DEB 包：

```
https://www.segger.com/downloads/jlink/
```

安装：

```bash
sudo dpkg -i JLink_Linux_Vxxx_x86_64.deb

# 验证安装
JLinkExe -Version
```

### 2.3 硬件连接

1. 将 J-Link 调试器接入宿主机的 USB 口
2. 在虚拟机软件中将 J-Link USB 设备**直通**到虚拟机
3. J-Link 的 SWD 接口连接 S32K144 开发板（SWDIO / SWCLK / GND）
4. 确保开发板已供电

---

## 3. 工程结构

```
Domain_Controller/
├── mcu/
│   ├── s32k144_flash.ld       # 链接脚本（Flash/SRAM 布局）
│   ├── gpio/                  # GPIO 驱动模块
│   ├── uart/                  # UART 驱动模块
│   ├── timer/                 # Timer 驱动模块
│   ├── adc/                   # ADC 驱动模块
│   ├── flexcan/               # FlexCAN 驱动模块
│   └── clock/                 # Clock 驱动模块
├── tools/
│   ├── mcu_flash.sh           # 一键编译+烧录脚本（推荐方式）
│   └── soc_build.sh           # SOC 端（Linux 域控制器）构建脚本
└── docs/
    └── MCU_交叉编译与烧录指南.md  # 本文档
```

### 3.1 链接脚本（mcu/s32k144_flash.ld）

S32K144 的内存布局：

| 区域 | 地址范围 | 大小 |
|------|----------|------|
| Flash | 0x00000000 - 0x0007FFFF | 512KB |
| SRAM | 0x1FFF8000 - 0x20003FFF | 48KB |

---

## 4. 编译流程

每个驱动模块使用独立的 Makefile，编译流程一致。

### 4.1 Makefile 关键参数

以下以 GPIO 模块为例：

```makefile
CC      = arm-none-eabi-gcc
MCU     = cortex-m4
FPU     = -mfloat-abi=soft
CFLAGS  = -mcpu=$(MCU) $(FPU) -mthumb
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
CFLAGS += -O2 -g
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS = -mcpu=$(MCU) $(FPU) -mthumb
LDFLAGS += -T ../s32k144_flash.ld      # 使用链接脚本
LDFLAGS += -Wl,--gc-sections           # 垃圾回收，减小体积
LDFLAGS += -specs=nano.specs           # 使用精简 C 库
```

### 4.2 编译产物

| 格式 | 用途 | 生成命令 |
|------|------|----------|
| `.elf` | ELF 可执行文件（含调试信息） | `arm-none-eabi-gcc` 链接产出 |
| `.hex` | Intel HEX 格式（烧录用） | `arm-none-eabi-objcopy -O ihex` |
| `.bin` | 纯二进制镜像 | `arm-none-eabi-objcopy -O binary` |

### 4.3 手动编译（以 GPIO 为例）

```bash
# 进入模块目录
cd mcu/gpio

# 清理
make clean

# 编译
make -j$(nproc)

# 产物
ls -la gpio_demo.elf gpio_demo.hex gpio_demo.bin
```

---

## 5. 烧录方式

### 5.1 方式一：推荐 — 一键脚本

```bash
# 仅编译
./tools/mcu_flash.sh gpio

# 编译并烧录
./tools/mcu_flash.sh gpio flash
```

支持的所有模块：

```bash
./tools/mcu_flash.sh gpio flash        # GPIO
./tools/mcu_flash.sh uart flash        # UART
./tools/mcu_flash.sh timer flash       # Timer
./tools/mcu_flash.sh adc flash         # ADC
./tools/mcu_flash.sh flexcan flash     # FlexCAN
./tools/mcu_flash.sh clock flash       # Clock
```

### 5.2 方式二：直接调用 Makefile flash 目标

```bash
cd mcu/gpio
make flash
```

每个 Makefile 的 `flash` 目标内部执行：

```bash
printf "loadfile gpio_demo.hex\nr\ng\nq\n" | \
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1
```

### 5.3 JLinkExe 命令详解

烧录命令由以下参数组成：

| 参数 | 说明 | 值 |
|------|------|-----|
| `-device` | 目标芯片型号 | S32K144 |
| `-if` | 调试接口 | SWD |
| `-speed` | SWD 时钟频率（kHz） | 4000 |
| `-autoconnect` | 自动连接目标 | 1 |

输入的 J-Link 命令：

```
loadfile gpio_demo.hex   # 加载 hex 文件到 Flash
r                        # 复位 MCU
g                        # 运行程序
q                        # 退出 JLinkExe
```

---

## 6. 模块清单

| 模块 | 目录 | 目标名 | 对应书籍章节 |
|------|------|--------|-------------|
| GPIO | `mcu/gpio/` | `gpio_demo` | 第4章 GPIO及程序框架 |
| UART | `mcu/uart/` | `uart_demo` | 第6章 串口通信 |
| Timer | `mcu/timer/` | `timer_demo` | 第7章 定时器 |
| ADC | `mcu/adc/` | `adc_demo` | 第10章 ADC模块 |
| FlexCAN | `mcu/flexcan/` | `flexcan_demo` | 第11章 CAN总线 |
| Clock | `mcu/clock/` | `clock_demo` | 第5章 时钟系统 |

---

## 7. 常用命令速查

```bash
# === 编译相关 ===

# 编译单个模块
./tools/mcu_flash.sh gpio

# 清理后重新编译
make -C mcu/gpio clean
make -C mcu/gpio -j$(nproc)

# 查看编译产物大小
arm-none-eabi-size mcu/gpio/gpio_demo.elf

# 反汇编查看生成代码
arm-none-eabi-objdump -d mcu/gpio/gpio_demo.elf


# === 烧录相关 ===

# 一键编译+烧录
./tools/mcu_flash.sh gpio flash

# 单独烧录（需先编译）
make -C mcu/gpio flash

# 手动执行 JLinkExe 烧录
printf "loadfile mcu/gpio/gpio_demo.hex\nr\ng\nq\n" | \
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1
```

---

## 8. 常见问题

### Q1: 编译时报 `arm-none-eabi-gcc: command not found`

**原因**：未安装交叉编译工具链。
**解决**：
```bash
sudo apt install gcc-arm-none-eabi
```

### Q2: 烧录时报 `Could not find J-Link`

**原因**：
- J-Link 未插入 USB
- 虚拟机未直通 J-Link USB 设备
**解决**：
```bash
# 检查 USB 设备是否识别
lsusb | grep -i segger

# 检查设备节点
ls /dev/ttyACM*
```

### Q3: 烧录时报 `Cannot connect to target`

**原因**：SWD 连接问题，检查 SWDIO/SWCLK/GND 三根线是否正确连接，开发板是否供电。

### Q4: 编译有 warning 但能生成 hex 文件

当前工程中存在 `struct has no members` 和 `_close is not implemented` 等 warning，不影响烧录和运行。这些是教学示例代码中预留的结构体骨架和裸机未实现的系统调用。

---

## 9. 附录：相关文件

| 文件 | 说明 |
|------|------|
| `mcu/s32k144_flash.ld` | S32K144 链接脚本（内存布局） |
| `tools/mcu_flash.sh` | 一键编译+烧录脚本 |
| `mcu/*/Makefile` | 各模块构建脚本 |
| `mcu/*/include/*.h` | 各模块驱动头文件 |
| `mcu/*/src/*.c` | 各模块驱动实现 |
