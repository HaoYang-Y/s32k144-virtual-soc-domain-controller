/**
 * @file    Gpio.h
 * @brief   AUTOSAR CP MCAL Gpio 驱动 — GPIO 读写封装
 *
 * @note    对标 AUTOSAR SWS_Gpio:
 *          - SWS_Gpio_00013: Gpio_Init
 *          - SWS_Gpio_00013: Gpio_WriteChannel
 *          - SWS_Gpio_00014: Gpio_ReadChannel
 *          - SWS_Gpio_00015: Gpio_FlipChannel
 *          底层调用 NXP S32 SDK PINS_DRV API，头文件不暴露任何 SDK 类型
 */

#ifndef MCAL_GPIO_H
#define MCAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "Std_Types.h"

typedef uint16_t Gpio_ChannelType;
typedef bool     Gpio_PinLevelType;

typedef enum {
    GPIO_DIR_INPUT  = 0,
    GPIO_DIR_OUTPUT = 1,
} Gpio_DirectionType;

typedef enum {
    GPIO_PULL_DISABLE = 0,
    GPIO_PULL_DOWN    = 1,
    GPIO_PULL_UP      = 2,
} Gpio_PullType;

typedef struct {
    Gpio_ChannelType   channel;
    Gpio_DirectionType direction;
    Gpio_PullType       pull;
} Gpio_ChannelConfigType;

typedef struct {
    uint8_t                      num_channels;
    const Gpio_ChannelConfigType *channels;
} Gpio_ConfigType;

#define GPIO_CH(port, pin) \
    ((Gpio_ChannelType)(((uint16_t)(port) << 8) | (uint16_t)(pin)))

/* ===================================================================
 *  API 函数声明 (AUTOSAR SWS_Gpio)
 * =================================================================== */

/** @brief 初始化 GPIO 驱动 (SWS_Gpio_00013) */
Std_ReturnType Gpio_Init(const Gpio_ConfigType *cfg);

/** @brief 读取通道电平 (SWS_Gpio_00014) */
Std_ReturnType Gpio_ReadChannel(Gpio_ChannelType channel,
                                Gpio_PinLevelType *level);

/** @brief 设置通道输出电平 (SWS_Gpio_00013) */
Std_ReturnType Gpio_WriteChannel(Gpio_ChannelType channel,
                                 Gpio_PinLevelType level);

/** @brief 翻转通道输出电平 (SWS_Gpio_00015) */
Std_ReturnType Gpio_FlipChannel(Gpio_ChannelType channel);

#endif /* MCAL_GPIO_H */
