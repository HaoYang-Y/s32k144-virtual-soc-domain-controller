# =====================================================================
# Timer 模块 — S32K144 定时器驱动练习 (SysTick + LPIT)
# 对应书籍：第7章 定时器
# =====================================================================
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

MCU     = cortex-m4
FPU     = -mfloat-abi=soft

CFLAGS  = -mcpu=$(MCU) $(FPU) -mthumb
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
CFLAGS += -O2 -g
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -I. -I./include

LDFLAGS = -mcpu=$(MCU) $(FPU) -mthumb
LDFLAGS += -T ../s32k144_flash.ld
LDFLAGS += -Wl,--gc-sections -Wl,-Map=output.map
LDFLAGS += -specs=nano.specs -specs=nosys.specs

SRCS  = src/main.c
SRCS += src/timer_driver.c

OBJS  = $(SRCS:.c=.o)
TARGET = timer_demo.elf
HEX    = $(TARGET:.elf=.hex)
BIN    = $(TARGET:.elf=.bin)

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
