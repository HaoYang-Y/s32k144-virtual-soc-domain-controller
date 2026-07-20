# =====================================================================
# MCU Makefile — S32K144 AUTOSAR Classic Platform 固件
# 基于 NXP S32 SDK for S32K1xx (RTM 4.0.2)
# =====================================================================
CC      := arm-none-eabi-gcc
AS      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size
RM      := rm -f

# ---------- 目标芯片 ----------
MCU     := cortex-m4
FPU     := -mfloat-abi=hard -mfpu=fpv4-sp-d16

# ---------- SDK 根目录 ----------
SDK_DIR := S32_SDK_S32K1xx_RTM_4.0.2

# ---------- 头文件路径 (AUTOSAR 模块 include) ----------
INCLUDES  := -I./include
INCLUDES  += -I./MCAL/Can/include
INCLUDES  += -I./MCAL/Gpio/include
INCLUDES  += -I./MCAL/Mcu/include
INCLUDES  += -I./MCAL/Port/include
INCLUDES  += -I./MCAL/Spi/include
INCLUDES  += -I./EcuAbstraction/CanIf/include
INCLUDES  += -I./EcuAbstraction/SpiIf/include
INCLUDES  += -I./Services/Com/include
INCLUDES  += -I./Services/PduR/include
INCLUDES  += -I./Services/CanTp/include
INCLUDES  += -I./Services/EcuM/include
INCLUDES  += -I./Services/BswM/include
INCLUDES  += -I./App/Swc_SignalGateway/include
INCLUDES  += -I./CDD/Uart/include
INCLUDES  += -I./Services/Log/include
INCLUDES  += -I./RTE

# ---------- 头文件路径 (AUTOSAR 模块 config) ----------
INCLUDES  += -I./MCAL/Can/config
INCLUDES  += -I./MCAL/Gpio/config
INCLUDES  += -I./MCAL/Mcu/config
INCLUDES  += -I./MCAL/Port/config
INCLUDES  += -I./MCAL/Spi/config
INCLUDES  += -I./EcuAbstraction/CanIf/config
INCLUDES  += -I./EcuAbstraction/SpiIf/config
INCLUDES  += -I./Services/Com/config
INCLUDES  += -I./Services/PduR/config
INCLUDES  += -I./Services/CanTp/config
INCLUDES  += -I./App/Swc_SignalGateway/config
INCLUDES  += -I./CDD/Uart/config
INCLUDES  += -I./Services/Log/config

# ---------- 头文件路径 (SDK) ----------
INCLUDES  += -I$(SDK_DIR)/platform/devices
INCLUDES  += -I$(SDK_DIR)/platform/devices/S32K144
INCLUDES  += -I$(SDK_DIR)/platform/devices/S32K144/include
INCLUDES  += -I$(SDK_DIR)/platform/devices/common
INCLUDES  += -I$(SDK_DIR)/platform/drivers/inc
INCLUDES  += -I$(SDK_DIR)/platform/drivers/src/pins
INCLUDES  += -I$(SDK_DIR)/platform/drivers/src/clock/S32K1xx
INCLUDES  += -I$(SDK_DIR)/platform/drivers/src/flexcan
INCLUDES  += -I$(SDK_DIR)/platform/drivers/src/edma
INCLUDES  += -I$(SDK_DIR)/platform/drivers/src/lpuart
INCLUDES  += -I$(SDK_DIR)/platform/drivers/src/interrupt
INCLUDES  += -I$(SDK_DIR)/platform/pal/inc
INCLUDES  += -I$(SDK_DIR)/platform/pal/src/can
INCLUDES  += -I$(SDK_DIR)/rtos/osif

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
CFLAGS  += -DFEATURE_CAN_HAS_DMA_ENABLE=0
CFLAGS  += $(INCLUDES)

# 汇编标志
ASFLAGS := -x assembler-with-cpp $(INCLUDES)

# ---------- 构建目录 ----------
BUILD_DIR := build

# ---------- 链接标志 ----------
LDFLAGS := -mcpu=$(MCU) $(FPU) -mthumb
LDFLAGS += -T $(SDK_DIR)/platform/devices/S32K144/linker/gcc/S32K144_64_flash.ld
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/output.map
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --entry=Reset_Handler
LDFLAGS += -nostartfiles -nodefaultlibs -nostdlib
LDFLAGS += -lgcc -lm -lc -lnosys
LDFLAGS += --specs=nano.specs --specs=nosys.specs

# ---------- 用户源文件 ----------
SRCS_USER  := App/Swc_SignalGateway/src/main.c

# MCAL 层
SRCS_USER  += MCAL/Can/src/Can.c
SRCS_USER  += MCAL/Gpio/src/Gpio.c
SRCS_USER  += MCAL/Mcu/src/Mcu.c
SRCS_USER  += MCAL/Port/src/pin_mux.c
SRCS_USER  += MCAL/Port/src/Port.c
SRCS_USER  += MCAL/Mcu/src/clock_config.c
# SRCS_USER  += MCAL/stubs.c  (已删除, 编译器运行时库自行提供 memset/memcpy)

# CDD 复杂设备驱动
SRCS_USER  += CDD/Uart/src/Uart.c

# Services 服务层
SRCS_USER  += Services/Log/src/Log.c

# ECU Abstraction 层
SRCS_USER  += EcuAbstraction/CanIf/src/CanIf.c
SRCS_USER  += EcuAbstraction/CanIf/config/CanIf_Cfg.c

# MCAL 骨架源文件 (待实现)
#SRCS_USER  += MCAL/Spi/src/Spi.c

# ECU Abstraction 层骨架源文件 (待实现)
#SRCS_USER  += EcuAbstraction/SpiIf/src/SpiIf.c

# Services 层骨架源文件 (待实现)
SRCS_USER  += Services/EcuM/src/EcuM.c
#SRCS_USER  += Services/Com/src/Com.c
#SRCS_USER  += Services/PduR/src/PduR.c
#SRCS_USER  += Services/CanTp/src/CanTp.c
#SRCS_USER  += Services/BswM/src/BswM.c

# RTE 骨架源文件 (待实现)
#SRCS_USER  += RTE/Rte.c

# App 层源文件
SRCS_USER  += App/Swc_SignalGateway/src/Swc_SignalGateway.c

# SDK 驱动源文件
SRCS_SDK_DRV  := $(SDK_DIR)/platform/drivers/src/pins/pins_driver.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/pins/pins_port_hw_access.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/flexcan/flexcan_driver.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/flexcan/flexcan_irq.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/flexcan/flexcan_hw_access.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/interrupt/interrupt_manager.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/lpuart/lpuart_driver.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/lpuart/lpuart_hw_access.c
SRCS_SDK_DRV  += $(SDK_DIR)/platform/drivers/src/lpuart/lpuart_irq.c


# SDK EDMA 驱动源文件 (卖家链接了完整 EDMA, 不用 stub)
SRCS_SDK_EDMA := $(SDK_DIR)/platform/drivers/src/edma/edma_driver.c
SRCS_SDK_EDMA += $(SDK_DIR)/platform/drivers/src/edma/edma_hw_access.c
SRCS_SDK_EDMA += $(SDK_DIR)/platform/drivers/src/edma/edma_irq.c

# SDK CAN PAL 源文件
SRCS_SDK_PAL := $(SDK_DIR)/platform/pal/src/can/can_pal.c

# SDK OSIF 源文件 (BareMetal 版本，不使用 FreeRTOS)
SRCS_SDK_OSIF := $(SDK_DIR)/rtos/osif/osif_baremetal.c

# SDK 启动和系统文件
SRCS_SDK  := $(SDK_DIR)/platform/devices/startup.c
SRCS_SDK  += $(SDK_DIR)/platform/devices/S32K144/startup/system_S32K144.c

# SDK 汇编启动文件
ASM_SDK  := $(SDK_DIR)/platform/devices/S32K144/startup/gcc/startup_S32K144.S

# 汇总所有源文件和生成的目标文件
ALL_SRCS := $(SRCS_USER) $(SRCS_SDK_DRV) $(SRCS_SDK_EDMA) $(SRCS_SDK_PAL) $(SRCS_SDK_OSIF) $(SRCS_SDK)

OBJDIR    := $(BUILD_DIR)/obj

OBJS      := $(addprefix $(OBJDIR)/, $(ALL_SRCS:.c=.o))
OBJS      += $(addprefix $(OBJDIR)/, $(ASM_SDK:.S=.o))

TARGET    := $(BUILD_DIR)/mcu_demo.elf
HEX       := $(BUILD_DIR)/mcu_demo.hex
BIN       := $(BUILD_DIR)/mcu_demo.bin

# ---------- 目标 ----------
.PHONY: all clean flash

all: $(TARGET) $(HEX) $(BIN)

# 确保构建目录存在
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# C 文件 → build/obj/<路径>/xxx.o
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# 汇编文件 → build/obj/<路径>/xxx.o
$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c -o $@ $<

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
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
	$(RM) -r $(BUILD_DIR)
