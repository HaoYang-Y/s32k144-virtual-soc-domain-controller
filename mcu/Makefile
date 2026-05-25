# =====================================================================
# MCU Makefile — S32K144 域控制器
# 基于 NXP S32 SDK for S32K1xx (RTM 4.0.2)
# =====================================================================
CC      := arm-none-eabi-gcc
AS      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size
RM      := rm -f

# ---------- 目标芯片 ----------
MCU     := cortex-m4
FPU     := -mfloat-abi=soft

# ---------- SDK 根目录 ----------
SDK_DIR := S32_SDK_S32K1xx_RTM_4.0.2

# ---------- 头文件路径 ----------
INCLUDES  := -I./include
INCLUDES  += -I$(SDK_DIR)/platform/devices
INCLUDES  += -I$(SDK_DIR)/platform/devices/S32K144
INCLUDES  += -I$(SDK_DIR)/platform/devices/S32K144/include
INCLUDES  += -I$(SDK_DIR)/platform/devices/common

# ---------- 编译标志 ----------
CFLAGS  := -mcpu=$(MCU) $(FPU) -mthumb
CFLAGS  += -Wall -Wextra -Wpedantic
CFLAGS  += -O0 -g3
CFLAGS  += -ffunction-sections -fdata-sections -fno-jump-tables
CFLAGS  += -funsigned-char -funsigned-bitfields -fshort-enums
CFLAGS  += -std=gnu99
CFLAGS  += -DCPU_S32K144HFT0VLLT
CFLAGS  += -DSTART_FROM_FLASH
CFLAGS  += -DDISABLE_WDOG
CFLAGS  += -DENABLE_FPU
CFLAGS  += $(INCLUDES)

# 汇编标志
ASFLAGS := -x assembler-with-cpp $(INCLUDES)

# ---------- 链接标志 ----------
LDFLAGS := -mcpu=$(MCU) $(FPU) -mthumb
LDFLAGS += -T $(SDK_DIR)/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld
LDFLAGS += -Wl,-Map=output.map
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --entry=Reset_Handler
LDFLAGS += -nostartfiles -nodefaultlibs -nostdlib
LDFLAGS += -lgcc -lm -lc -lrdimon
LDFLAGS += --specs=rdimon.specs

# ---------- 源文件 ----------
# 用户源文件
SRCS_USER  := src/main.c
SRCS_USER  += src/gpio.c
# 以下模块写了但还未调试通，后续逐步启用
# SRCS_USER  += src/adc.c
# SRCS_USER  += src/clock.c
# SRCS_USER  += src/timer.c
# SRCS_USER  += src/uart.c
# SRCS_USER  += src/flexcan.c

# SDK 启动和系统文件
SRCS_SDK  := $(SDK_DIR)/platform/devices/startup.c
SRCS_SDK  += $(SDK_DIR)/platform/devices/S32K144/startup/system_S32K144.c

# SDK 汇编启动文件
ASM_SDK  := $(SDK_DIR)/platform/devices/S32K144/startup/gcc/startup_S32K144.S

# 汇总所有源文件和生成的目标文件
ALL_SRCS := $(SRCS_USER) $(SRCS_SDK)
OBJS     := $(ALL_SRCS:.c=.o)
OBJS     += $(ASM_SDK:.S=.o)

TARGET   := mcu_demo.elf
HEX      := $(TARGET:.elf=.hex)
BIN      := $(TARGET:.elf=.bin)

# ---------- 目标 ----------
.PHONY: all clean flash

all: $(TARGET) $(HEX) $(BIN)

# C 文件编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 汇编文件编译规则
%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(HEX): $(TARGET)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(TARGET)
	$(OBJCOPY) -O binary $< $@

flash: $(HEX)
	@echo "[FLASH] 使用 J-Link 烧录 $< ..."
	printf "loadfile %s\nr\ng\nq\n" "$<" | \
	JLinkExe -device S32K144 -if SWD -speed 4000 -autoconnect 1

clean:
	$(RM) $(TARGET) $(HEX) $(BIN) output.map
	find . -name "*.o" -exec $(RM) {} \;
