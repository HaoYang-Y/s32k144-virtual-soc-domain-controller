# S32K144 交叉编译与 J-Link 烧录指南

## 1. 概述

本文档说明如何将 Domain_Controller 工程中的 MCU 模块交叉编译为 S32K144 可执行文件，
并通过 J-Link 烧录到开发板。

**整体工作流：**

```
源码 (*.c / *.S)  →  arm-none-eabi-gcc 交叉编译  →  .elf / .hex  →  JLinkExe 烧录 → S32K144
```

**SDK 说明：** 本工程 MCU 代码基于 NXP S32 SDK (RTM 4.0.2) 开发，
SDK 位于 `mcu/S32_SDK_S32K1xx_RTM_4.0.2/`。SDK 提供完整的 HAL/PD 驱动
和设备头文件，无需直接操作寄存器。

---

## 2. 环境准备

### 2.1 交叉编译工具链

安装 `arm-none-eabi-gcc`（ARM Cortex-M4 交叉编译器）：

```bash
# Ubuntu / Debian
sudo apt install gcc-arm-none-eabi

# 验证安装
arm-none-eabi-gcc --version
```

工具链基于 Cortex-M4F 内核，使用软浮点（`-mfloat-abi=soft`）。

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
4. 确保开发板已供电（S32K144 EVB 通过 USB 供电）

---

## 3. 工程结构

```
Domain_Controller/
├── mcu/
│   ├── s32k144_flash.ld          # 链接脚本（Flash/SRAM 布局）
│   ├── Makefile                  # 顶层编译脚本，编译所有外设驱动
│   ├── include/                  # 各外设驱动头文件
│   │   ├── gpio.h                # GPIO 驱动 API
│   │   ├── uart.h                # UART 驱动 API
│   │   ├── timer.h               # Timer 驱动 API
│   │   ├── adc.h                 # ADC 驱动 API
│   │   ├── flexcan.h             # FlexCAN 驱动 API
│   │   └── clock.h               # 时钟配置 API
│   ├── src/                      # 各外设驱动源文件
│   │   ├── gpio.c                # GPIO 驱动实现
│   │   ├── uart.c                # UART 驱动实现
│   │   ├── timer.c               # Timer 驱动实现
│   │   ├── adc.c                 # ADC 驱动实现
│   │   ├── flexcan.c             # FlexCAN 驱动实现
│   │   └── clock.c               # 时钟配置实现
│   └── S32_SDK_S32K1xx_RTM_4.0.2/  # NXP S32 SDK（预置）
│       └── platform/
│           ├── drivers/inc/      # SDK 外设驱动头文件
│           ├── devices/          # 设备寄存器定义
│           ├── startup/          # 启动文件
│           └── ...
├── tools/
│   ├── mcu_flash.sh              # 一键编译+烧录脚本
│   └── soc_build.sh              # SOC 端构建脚本
└── docs/
    └── MCU_交叉编译与烧录指南.md   # 本文档
```

### 3.1 链接脚本（mcu/s32k144_flash.ld）

S32K144 的内存布局：

| 区域 | 地址范围 | 大小 |
|------|----------|------|
| Flash | 0x00000000 - 0x0007FFFF | 512 KB |
| SRAM | 0x1FFF8000 - 0x20003FFF | 48 KB |

链接脚本定义了代码段、数据段、BSS 段等在 Flash 和 SRAM 中的放置位置。

---

## 4. 编译流程

### 4.1 Makefile 结构

使用单个顶层 Makefile（`mcu/Makefile`）编译选定的外设驱动模块。
通过 `TARGET` 变量指定要编译的模块。

**SDK 路径约定**：NXP S32 SDK 位于 `S32_SDK_S32K1xx_RTM_4.0.2/`
（相对于 `mcu/Makefile` 所在目录），所有头文件和启动文件通过相对路径引用。

### 4.2 Makefile 关键参数

```makefile
# mcu/Makefile 核心配置

CC      = arm-none-eabi-gcc
MCU     = cortex-m4
FPU     = -mfloat-abi=soft

CFLAGS  = -mcpu=$(MCU) $(FPU) -mthumb
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
CFLAGS += -O2 -g
CFLAGS += -ffunction-sections -fdata-sections

# SDK 基础路径
SDK_BASE = S32_SDK_S32K1xx_RTM_4.0.2

# SDK 头文件搜索路径
CFLAGS += -I$(SDK_BASE)/platform/drivers/inc
CFLAGS += -I$(SDK_BASE)/platform/devices
CFLAGS += -I$(SDK_BASE)/platform/devices/S32K144

# SDK 启动文件
SDK_STARTUP = $(SDK_BASE)/platform/startup/startup_S32K144.S

LDFLAGS  = -mcpu=$(MCU) $(FPU) -mthumb
LDFLAGS += -T s32k144_flash.ld
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -specs=nano.specs
LDFLAGS += -specs=nosys.specs
```

### 4.3 编译逻辑

```makefile
# 通过 TARGET 选择编译哪个模块
TARGET ?= flexcan

# 自动选择源文件（src/$(TARGET).c）加上 include 头文件、SDK 启动文件
SRC = src/$(TARGET).c
OBJ = $(TARGET).o

# 规则：编译源文件 + SDK 启动文件
$(TARGET).elf: $(TARGET).o $(SDK_STARTUP_O)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	arm-none-eabi-objcopy -O ihex $< $@
```

### 4.4 编译产物

| 格式 | 用途 | 生成方式 |
|------|------|---------|
| `.elf` | ELF 可执行文件（含调试信息） | `make` 默认产出 |
| `.hex` | Intel HEX 格式（烧录用） | `make` 自动生成 |
| `.bin` | 纯二进制镜像 | `arm-none-eabi-objcopy -O binary` |

### 4.5 手动编译

```bash
# 进入 mcu 目录
cd mcu

# 指定要编译的目标（默认为 flexcan）
make TARGET=gpio

# 编译其他模块
make TARGET=uart
make TARGET=flexcan
make TARGET=timer
make TARGET=adc
make TARGET=clock

# 清理
make clean
```

---

## 5. 烧录方式

### 5.1 推荐方式：一键脚本

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

### 5.2 直接通过 Makefile 烧录

```bash
cd mcu

# 编译 flexcan 并烧录
make TARGET=flexcan flash
```

`make flash` 目标内部执行（以 flexcan 为例）：

```bash
# 实际由 Makefile 中的 flash 目标执行：
printf "loadfile flexcan.hex\nr\ng\nq\n" | \
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1
```

### 5.3 JLinkExe 命令详解

| 参数 | 说明 | 值 |
|------|------|-----|
| `-device` | 目标芯片型号 | S32K144 |
| `-if` | 调试接口 | SWD |
| `-speed` | SWD 时钟频率（kHz） | 4000 |
| `-autoconnect` | 自动连接目标 | 1 |

J-Link 内部命令：

```
loadfile flexcan.hex   # 加载 hex 文件到 Flash
r                      # 复位 MCU
g                      # 运行程序
q                      # 退出 JLinkExe
```

---

## 6. 模块清单

| 模块 | TARGET 值 | 源文件 | 头文件 |
|------|-----------|--------|--------|
| GPIO | `gpio` | `src/gpio.c` | `include/gpio.h` |
| UART | `uart` | `src/uart.c` | `include/uart.h` |
| Timer | `timer` | `src/timer.c` | `include/timer.h` |
| ADC | `adc` | `src/adc.c` | `include/adc.h` |
| FlexCAN | `flexcan` | `src/flexcan.c` | `include/flexcan.h` |
| Clock | `clock` | `src/clock.c` | `include/clock.h` |

---

## 7. 常用命令速查

```bash
# === 编译 ===

# 编译 FlexCAN 模块
cd mcu && make TARGET=flexcan

# 编译所有模块（依次执行）
for t in gpio uart timer adc flexcan clock; do make TARGET=$t; done

# 查看编译产物大小
arm-none-eabi-size flexcan.elf

# 反汇编查看机器码
arm-none-eabi-objdump -d flexcan.elf

# === 烧录 ===

# 一键编译+烧录
./tools/mcu_flash.sh flexcan flash

# 单独烧录（需先编译）
cd mcu && make TARGET=flexcan flash

# 手动 JLinkExe 烧录
printf "loadfile flexcan.hex\nr\ng\nq\n" | \
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1
```

---

## 8. 常见问题

### Q1: `arm-none-eabi-gcc: command not found`

**原因**：未安装交叉编译工具链。
**解决**：
```bash
sudo apt install gcc-arm-none-eabi
```

### Q2: `Could not find J-Link`

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

### Q3: `Cannot connect to target`

**原因**：SWD 连接问题——检查 SWDIO / SWCLK / GND 三根线是否正确连接，
开发板是否供电。

### Q4: 编译有 warning 但能生成 hex 文件

工程中存在 `struct has no members` 和 `_close is not implemented` 等 warning，
不影响烧录和运行。这些是 SDK 骨架结构体和裸机下未实现的系统调用桩。

---

## 9. 附录：相关文件

| 文件 | 说明 |
|------|------|
| `mcu/s32k144_flash.ld` | S32K144 链接脚本（内存布局） |
| `tools/mcu_flash.sh` | 一键编译+烧录脚本 |
| `mcu/Makefile` | 顶层构建脚本 |
| `mcu/include/*.h` | 各模块驱动头文件 |
| `mcu/src/*.c` | 各模块驱动实现 |
| `mcu/S32_SDK_S32K1xx_RTM_4.0.2/` | NXP S32 SDK（预置） |

> **注意**：本工程采用编译 SDK 源码的方式（包含 `startup_S32K144.S`），
> 而非链接预编译库，好处是代码完全可控、便于调试。
