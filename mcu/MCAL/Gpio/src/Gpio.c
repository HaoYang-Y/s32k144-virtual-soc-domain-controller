/**
 * @file    Gpio.c
 * @brief   AUTOSAR CP MCAL Gpio 驱动实现
 */

#include "Gpio.h"
#include "device_registers.h"
#include "pins_driver.h"
#include <stddef.h>

static GPIO_Type *const gpio_base[] = { PTA, PTB, PTC, PTD, PTE };
static PORT_Type *const port_base[] = { PORTA, PORTB, PORTC, PORTD, PORTE };

static uint8_t channel_port(Gpio_ChannelType ch) { return (uint8_t)(ch >> 8); }
static uint8_t channel_pin(Gpio_ChannelType ch)  { return (uint8_t)(ch & 0xFFu); }

static bool channel_valid(Gpio_ChannelType c) {
    return (channel_port(c) <= 4U) && (channel_pin(c) <= 31U);
}

static port_pull_config_t convert_pull(Gpio_PullType pull) {
    switch (pull) {
        case GPIO_PULL_UP:   return PORT_INTERNAL_PULL_UP_ENABLED;
        case GPIO_PULL_DOWN: return PORT_INTERNAL_PULL_DOWN_ENABLED;
        default:             return PORT_INTERNAL_PULL_NOT_ENABLED;
    }
}

static port_data_direction_t convert_direction(Gpio_DirectionType dir) {
    return (dir == GPIO_DIR_OUTPUT) ? GPIO_OUTPUT_DIRECTION
                                     : GPIO_INPUT_DIRECTION;
}

/* ===================================================================
 *  Gpio_Init (SWS_Gpio_00013)
 * =================================================================== */

Std_ReturnType Gpio_Init(const Gpio_ConfigType *cfg)
{
    if ((cfg == NULL) || (cfg->channels == NULL) || (cfg->num_channels == 0U))
        return E_NOT_OK;

    for (uint8_t i = 0U; i < cfg->num_channels; i++) {
        const Gpio_ChannelConfigType *ch = &cfg->channels[i];
        if (!channel_valid(ch->channel)) continue;

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
    return E_OK;
}

/* ===================================================================
 *  Gpio_ReadChannel (SWS_Gpio_00014)
 * =================================================================== */

Std_ReturnType Gpio_ReadChannel(Gpio_ChannelType channel,
                                Gpio_PinLevelType *level)
{
    if (!channel_valid(channel) || (level == NULL)) return E_NOT_OK;

    pins_channel_type_t val = PINS_DRV_ReadPins(gpio_base[channel_port(channel)]);
    *level = (bool)((val >> channel_pin(channel)) & 1U);
    return E_OK;
}

/* ===================================================================
 *  Gpio_WriteChannel (SWS_Gpio_00013)
 * =================================================================== */

Std_ReturnType Gpio_WriteChannel(Gpio_ChannelType channel,
                                  Gpio_PinLevelType level)
{
    if (!channel_valid(channel)) return E_NOT_OK;

    PINS_DRV_WritePin(gpio_base[channel_port(channel)],
                      (pins_channel_type_t)channel_pin(channel),
                      (pins_level_type_t)level);
    return E_OK;
}

/* ===================================================================
 *  Gpio_FlipChannel (SWS_Gpio_00015)
 * =================================================================== */

Std_ReturnType Gpio_FlipChannel(Gpio_ChannelType channel)
{
    if (!channel_valid(channel)) return E_NOT_OK;

    GPIO_Type *gpio = gpio_base[channel_port(channel)];
    uint32_t pin = (uint32_t)channel_pin(channel);

    uint32_t reg = gpio->PDOR;
    if (reg & (1UL << pin)) {
        gpio->PCOR = (1UL << pin);
    } else {
        gpio->PSOR = (1UL << pin);
    }
    return E_OK;
}
