# S32K144 交叉编译与 J-Link 烧录指南

> 最后更新: 2026-05-25

## 1. 概述

本文档说明如何将 Domain_Controller 工程中的 MCU 模块交叉编译为 S32K144 可执行文件，
并通过 J-Link 烧录到开发板。

**整体工作流：**

```
源码 (*.c / *.S)  →  arm-none-eabi-gcc 交叉编译  →  mcu_demo.elf/.hex  →  make flash → S32K144
```

**SDK 说明：** 本工程 MCU 代码基于 NXP S32 SDK (RTM 4.0.2) 开发，
SDK 位于 `mcu/S32_SDK_S32K1xx_RTM_4.0.2/`。编译时使用 SDK 提供的启动文件
和官方链接脚本 `S32K144_64_flash.ld`。

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

工具链基于 Cortex-M4 内核，使用软浮点（`-mfloat-abi=soft`）。

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
│   ├── Makefile                  # 顶层构建脚本
│   ├── include/                  # 各外设驱动头文件
│   │   ├── gpio.h                # GPIO 驱动 API
│   │   ├── uart.h                # UART 驱动 API
│   │   ├── timer.h               # Timer 驱动 API
│   │   ├── adc.h                 # ADC 驱动 API
│   │   ├── flexcan.h             # FlexCAN 驱动 API
│   │   └── clock.h               # 时钟配置 API
│   ├── src/                      # 各外设驱动源文件
│   │   ├── main.c                # 主程序（LED 闪烁 demo）
│   │   ├── gpio.c                # GPIO 驱动实现（直接寄存器操作）
│   │   ├── uart.c                # UART 驱动实现
│   │   ├── timer.c               # Timer 驱动实现
│   │   ├── adc.c                 # ADC 驱动实现
│   │   ├── flexcan.c             # FlexCAN 驱动实现
│   │   └── clock.c               # 时钟配置实现
│   └── S32_SDK_S32K1xx_RTM_4.0.2/  # NXP S32 SDK（预置）
│       ├── platform/devices/
│       │   ├── startup.c                         # SDK C 启动代码
│       │   └── S32K144/
│       │       ├── startup/
│       │       │   ├── system_S32K144.c          # 系统初始化
│       │       │   └── gcc/startup_S32K144.S    # 汇编启动文件
│       │       └── linker/gcc/
│       │           └── S32K144_64_flash.ld      # 官方链接脚本 ⭐
│       └── ...
├── tools/
│   └── soc_build.sh              # SOC 端构建脚本
└── docs/
    └── MCU_交叉编译与烧录指南.md   # 本文档
```

### 3.1 链接脚本说明

使用 SDK 官方链接脚本 `S32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld`。

S32K144 的内存布局：

| 区域   | 地址范围                | 大小   |
|--------|------------------------|--------|
| Flash  | 0x00000000 - 0x0007FFFF | 512 KB |
| SRAM   | 0x1FFF8000 - 0x20003FFF | 48 KB  |

---

## 4. 编译流程

### 4.1 Makefile 结构

使用单个顶层 Makefile（`mcu/Makefile`），将所有启用的源文件编译为单一固件 `mcu_demo.elf`。

当前编译的源文件：

| 文件 | 说明 |
|------|------|
| `src/main.c` | 主程序（LED 交替闪烁） |
| `src/gpio.c` | GPIO 驱动（直接寄存器操作） |
| SDK `startup.c` | SDK C 启动代码 |
| SDK `system_S32K144.c` | 系统初始化 |
| SDK `startup_S32K144.S` | 汇编启动文件（中断向量表） |

后续模块（adc、uart、timer、flexcan、clock）已编写但未启用，在 Makefile 中以注释形式保留，
调试通过后逐步取消注释。

### 4.2 Makefile 关键参数

```makefile
# mcu/Makefile 核心配置

CC      = arm-none-eabi-gcc
MCU     = cortex-m4

CFLAGS  = -mcpu=$(MCU) -mfloat-abi=soft -mthumb
CFLAGS += -std=gnu99
CFLAGS += -Wall -Wextra -Wpedantic
CFLAGS += -O0 -g3
CFLAGS += -ffunction-sections -fdata-sections -fno-jump-tables
CFLAGS += -funsigned-char -funsigned-bitfields -fshort-enums
CFLAGS += -DCPU_S32K144HFT0VLLT -DSTART_FROM_FLASH -DDISABLE_WDOG

# SDK 头文件搜索路径
CFLAGS += -I./include
CFLAGS += -IS32_SDK_S32K1xx_RTM_4.0.2/platform/devices
CFLAGS += -IS32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144
CFLAGS += -IS32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/include
CFLAGS += -IS32_SDK_S32K1xx_RTM_4.0.2/platform/devices/common

# 链接脚本使用官方 SDK 版本
LDFLAGS += -T S32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld
LDFLAGS += -Wl,-Map=output.map -Wl,--gc-sections
LDFLAGS += --entry=Reset_Handler
LDFLAGS += -nostartfiles -nodefaultlibs -nostdlib
LDFLAGS += -lgcc -lm -lc -lrdimon --specs=rdimon.specs
```

### 4.3 编译命令

```bash
# 进入 mcu 目录
cd mcu

# 编译（生成 mcu_demo.elf / mcu_demo.hex / mcu_demo.bin）
make all

# 仅编译（默认目标也是 all）
make

# 清理
make clean
```

### 4.4 编译产物

| 格式   | 文件名            | 用途                        |
|--------|-------------------|----------------------------|
| `.elf` | `mcu_demo.elf`    | ELF 可执行文件（含调试信息） |
| `.hex` | `mcu_demo.hex`    | Intel HEX 格式（烧录用）     |
| `.bin` | `mcu_demo.bin`    | 纯二进制镜像                |
| `.map` | `output.map`      | 内存映射文件（调试用）       |

### 4.5 查看编译产物

```bash
# 查看段大小
arm-none-eabi-size mcu_demo.elf

# 反汇编
arm-none-eabi-objdump -d mcu_demo.elf

# 查看符号表
arm-none-eabi-nm mcu_demo.elf
```

---

## 5. 烧录流程

### 5.1 一键烧录（推荐）

```bash
cd mcu

# 编译并烧录
make all && make flash

# 或直接（flash 目标依赖 hex，会自动触发编译）
make flash
```

### 5.2 `make flash` 内部命令

```makefile
flash: $(HEX)
	printf "loadfile %s\nr\ng\nq\n" "$<" | \
	JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1
```

J-Link 内部命令说明：

```
loadfile mcu_demo.hex   # 加载 hex 文件到 Flash
r                      # 复位 MCU
g                      # 运行程序
q                      # 退出 JLinkExe
```

### 5.3 JLinkExe 参数

| 参数           | 说明             | 值        |
|----------------|------------------|-----------|
| `-device`      | 目标芯片型号     | S32K144   |
| `-if`          | 调试接口         | SWD       |
| `-speed`       | SWD 时钟 (kHz)   | 4000      |
| `-autoconnect` | 自动连接目标     | 1         |

### 5.4 手动烧录

```bash
# 先编译
cd mcu && make all

# 手动 JLinkExe
printf "loadfile mcu_demo.hex\nr\ng\nq\n" | \
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1
```

---

## 6. 模块清单

| 模块     | 源文件              | 头文件              | 状态       |
|----------|---------------------|---------------------|------------|
| Main     | `src/main.c`        | `gpio.h`            | ✅ 已验证  |
| GPIO     | `src/gpio.c`        | `include/gpio.h`    | 🔄 迁移中  |
| ADC      | `src/adc.c`         | `include/adc.h`     | ⏳ 待调试  |
| Clock    | `src/clock.c`       | `include/clock.h`   | ⏳ 待调试  |
| Timer    | `src/timer.c`       | `include/timer.h`   | ⏳ 待调试  |
| UART     | `src/uart.c`        | `include/uart.h`    | ⏳ 待调试  |
| FlexCAN  | `src/flexcan.c`     | `include/flexcan.h` | ⏳ 待调试  |

> **说明**：GPIO 驱动当前使用直接寄存器操作（`gpio.c`），
> 正在迁移到 SDK PINS_DRV API。其他外设模块将直接使用 SDK DRV 层 API 开发。

---

## 7. 常用命令速查

```bash
# === 编译 ===
cd mcu && make all          # 编译
cd mcu && make clean        # 清理

# 编译并查看产物大小
cd mcu && make all 2>&1 | tail -5

# 反汇编查看机器码
arm-none-eabi-objdump -d mcu/mcu_demo.elf | head -100

# === 烧录 ===
cd mcu && make flash        # 一键编译+烧录

# 仅烧录（需先编译）
cd mcu && printf "loadfile mcu_demo.hex\nr\ng\nq\n" | \
JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1

# === 调试 ===
# 查看编译产物大小
arm-none-eabi-size mcu/mcu_demo.elf
# 输出示例:
#    text    data     bss     dec     hex filename
#    2204       0    3072    5276    149c mcu_demo.elf
```

---

## 8. 常见问题

### Q1: `arm-none-eabi-gcc: command not found`

```bash
sudo apt install gcc-arm-none-eabi
```

### Q2: `Could not find J-Link`

```bash
# 检查 USB 设备是否识别
lsusb | grep -i segger

# 检查 JLinkExe 是否安装
which JLinkExe
```

### Q3: `Cannot connect to target`

- 检查 SWDIO / SWCLK / GND 三根线是否正确连接
- 检查开发板是否供电（VTref 应该约为 3.3V）
- 确保虚拟机已直通 J-Link USB 设备

### Q4: LED 看起来常亮不闪烁

`main.c` 中 `BLINK_DELAY_MS` 值过大。当前默认为 `500u`（500ms），
两个 LED 每 500ms 交替闪烁一次，视觉效果清晰可见。
如果修改延时值注意单位为毫秒。

### Q5: 编译有 warning 但能生成 hex 文件

SDK 的 `startup.c` 中存在一个 `-Warray-compare` 的 warning，
不影响编译和运行，可安全忽略。

---

## 9. 附录：相关文件

| 文件                                                              | 说明                        |
|-------------------------------------------------------------------|-----------------------------|
| `mcu/Makefile`                                                     | 顶层构建脚本                |
| `mcu/src/main.c`                                                   | 主程序（LED 闪烁 demo）      |
| `mcu/src/gpio.c`                                                   | GPIO 驱动                   |
| `mcu/include/gpio.h`                                               | GPIO 驱动头文件              |
| `mcu/S32_SDK_S32K1xx_RTM_4.0.2/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld` | 官方链接脚本 |
