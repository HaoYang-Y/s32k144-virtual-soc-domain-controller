/**
 * @file    Gpio_Cfg.h
 * @brief   GPIO 预编译配置 — 引脚到信号 ID 映射
 *
 * @note    配置所有 GPIO 输入/输出引脚的信号分配
 */

#ifndef GPIO_CFG_H
#define GPIO_CFG_H

#include "Std_Types.h"

/** @brief GPIO 信号 ID 枚举 */
typedef enum {
    GPIO_SIGNAL_BUTTON_1 = 0,
    GPIO_SIGNAL_BUTTON_2 = 1,
    GPIO_SIGNAL_LED_HEARTBEAT = 2,
    GPIO_SIGNAL_LED_ERROR     = 3,
    GPIO_SIGNAL_COUNT
} Gpio_SignalIdType;

/** @brief GPIO 通道配置 */
typedef struct {
    Gpio_SignalIdType signal_id;
    uint8_t           port;      /**< 0=A, 1=B, 2=C, 3=D, 4=E */
    uint8_t           pin;       /**< 0-31 */
    uint8_t           direction; /**< 0=输入, 1=输出 */
} Gpio_ChannelConfigType;

extern const Gpio_ChannelConfigType Gpio_ChannelConfig[];
extern const uint8_t                Gpio_ChannelConfig_Count;

#endif /* GPIO_CFG_H */