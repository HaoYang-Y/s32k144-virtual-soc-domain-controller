/**
 * @file    Gpio.c
 * @brief   AUTOSAR CP MCAL Gpio 驱动实现
 *
 * @note    内部调用 NXP S32 SDK PINS_DRV API 完成 GPIO 初始化和读写
 *          所有 SDK 依赖 (pins_driver.h, device_registers...) 仅在本 .c 内可见
 */

#include "Gpio.h"

/* SDK 依赖全部下沉到此文件内部 */
#include "device_registers.h"
#include "pins_driver.h"
#include <stddef.h>

/* ===================================================================
 *  SDK 基地址映射 (内部使用)
 * =================================================================== */

static GPIO_Type *const gpio_base[] = { PTA, PTB, PTC, PTD, PTE };
static PORT_Type *const port_base[] = { PORTA, PORTB, PORTC, PORTD, PORTE };

/* ===================================================================
 *  内部辅助函数
 * =================================================================== */

static uint8_t channel_port(Gpio_ChannelType ch) { return (uint8_t)(ch >> 8); }
static uint8_t channel_pin(Gpio_ChannelType ch)  { return (uint8_t)(ch & 0xFFu); }

static int check_channel(Gpio_ChannelType channel) {
    uint8_t port = channel_port(channel);
    uint8_t pin  = channel_pin(channel);
    if (port > 4 || pin > 31) return -1;
    return 0;
}

/** @brief MCAL 上拉类型 → SDK pull_config */
static port_pull_config_t convert_pull(Gpio_PullType pull) {
    switch (pull) {
        case GPIO_PULL_UP:      return PORT_INTERNAL_PULL_UP_ENABLED;
        case GPIO_PULL_DOWN:    return PORT_INTERNAL_PULL_DOWN_ENABLED;
        default:                return PORT_INTERNAL_PULL_NOT_ENABLED;
    }
}

/** @brief MCAL 方向 → SDK direction */
static port_data_direction_t convert_direction(Gpio_DirectionType dir) {
    return (dir == GPIO_DIR_OUTPUT) ? GPIO_OUTPUT_DIRECTION
                                     : GPIO_INPUT_DIRECTION;
}

/* ===================================================================
 *  Gpio_Init
 * =================================================================== */

void Gpio_Init(const Gpio_ConfigType *cfg)
{
    if ((cfg == NULL) || (cfg->channels == NULL) || (cfg->num_channels == 0))
        return;

    for (uint8_t i = 0; i < cfg->num_channels; i++) {
        const Gpio_ChannelConfigType *ch = &cfg->channels[i];
        if (check_channel(ch->channel) != 0) continue;

        pin_settings_config_t sdk_cfg;
        sdk_cfg.base          = port_base[channel_port(ch->channel)];
        sdk_cfg.pinPortIdx    = channel_pin(ch->channel);
        sdk_cfg.pullConfig    = convert_pull(ch->pull);
        sdk_cfg.mux           = PORT_MUX_AS_GPIO;
        sdk_cfg.gpioBase      = gpio_base[channel_port(ch->channel)];
        sdk_cfg.direction     = convert_direction(ch->direction);
        sdk_cfg.initValue     = 0U;
#if FEATURE_PINS_HAS_SLEW_RATE
        sdk_cfg.rateSelect    = PORT_FAST_SLEW_RATE;
#endif
#if FEATURE_PINS_HAS_OPEN_DRAIN
        sdk_cfg.openDrain     = PORT_OPEN_DRAIN_DISABLED;
#endif
#if FEATURE_PINS_HAS_DRIVE_STRENGTH
        sdk_cfg.driveSelect   = PORT_LOW_DRIVE_STRENGTH;
#endif
        sdk_cfg.intConfig     = PORT_INT_RISING_EDGE;
        sdk_cfg.clearIntFlag  = true;
        sdk_cfg.digitalFilter = false;

        (void)PINS_DRV_Init(1U, &sdk_cfg);
    }
}

/* ===================================================================
 *  Gpio_ReadPin
 * =================================================================== */

int Gpio_ReadPin(Gpio_ChannelType channel, Gpio_PinLevelType *level)
{
    if (check_channel(channel) != 0 || level == NULL) return -1;

    pins_channel_type_t val = PINS_DRV_ReadPins(gpio_base[channel_port(channel)]);
    *level = (bool)((val >> channel_pin(channel)) & 1U);
    return 0;
}

/* ===================================================================
 *  Gpio_WritePin
 * =================================================================== */

int Gpio_WritePin(Gpio_ChannelType channel, Gpio_PinLevelType level)
{
    if (check_channel(channel) != 0) return -1;

    PINS_DRV_WritePin(gpio_base[channel_port(channel)],
                      (pins_channel_type_t)channel_pin(channel),
                      (pins_level_type_t)level);
    return 0;
}