# =====================================================================
# MCU 统一 Makefile — S32K144 全部驱动模块
# 基于 NXP S32 SDK for S32K1xx (RTM 4.0.2)
# =====================================================================
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

MCU     = cortex-m4
FPU     = -mfloat-abi=soft

# ---------- SDK 路径 ----------
SDK_DIR         := S32_SDK_S32K1xx_RTM_4.0.2
SDK_DEVICES     := $(SDK_DIR)/platform/devices
SDK_DRIVERS_INC := $(SDK_DIR)/platform/drivers/inc

# ---------- 编译标志 ----------
CFLAGS  = -mcpu=$(MCU) $(FPU) -mthumb
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
CFLAGS += -O0 -g3
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -I./include
CFLAGS += -I$(SDK_DEVICES)
CFLAGS += -I$(SDK_DEVICES)/S32K144
CFLAGS += -I$(SDK_DRIVERS_INC)

# ---------- 链接标志 ----------
LDFLAGS = -mcpu=$(MCU) $(FPU) -mthumb
LDFLAGS += -T s32k144_flash.ld
LDFLAGS += -Wl,--gc-sections -Wl,-Map=output.map
LDFLAGS += -specs=nano.specs -specs=nosys.specs

# ---------- 源文件 ----------
SRCS  = src/gpio.c
SRCS += src/adc.c
SRCS += src/clock.c
SRCS += src/timer.c
SRCS += src/uart.c
SRCS += src/flexcan.c

OBJS   = $(SRCS:.c=.o)
TARGET = mcu_demo.elf
HEX    = $(TARGET:.elf=.hex)
BIN    = $(TARGET:.elf=.bin)

# ---------- 目标 ----------
.PHONY: all clean flash

all: $(TARGET) $(HEX) $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

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
	rm -f $(OBJS) $(TARGET) $(HEX) $(BIN) output.map
