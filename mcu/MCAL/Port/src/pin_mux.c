/**
 * @file    pin_mux.c
 * @brief   引脚复用配置实现 — 基于 SDK Pins Driver 标准结构体
 * @note    引脚列表:
 *          - PTD15 — GPIO Output (LED0)
 *          - PTD16 — GPIO Output (LED1)
 *          - PTE4  — CAN0_RX  (ALT5)
 *          - PTE5  — CAN0_TX  (ALT5)
 *          - PTA1  — LPUART0_RX (ALT2)
 *          - PTA2  — LPUART0_TX (ALT2)
 */

#include "pin_mux.h"

/*
 * 引脚配置数组
 * 顺序: LED0 → LED1 → CAN0_RX → CAN0_TX
 */
pin_settings_config_t g_pin_mux_InitConfigArr0[NUM_OF_CONFIGURED_PINS0] = {
    /* --- PTD0: 蓝灯 --- */
    {
        .base          = PORTD,
        .pinPortIdx    = 0U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 1U,     /* 初始高电平 (LED 灭) */
    },
    /* --- PTD1: 辅助灯 --- */
    {
        .base          = PORTD,
        .pinPortIdx    = 1U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 1U,     /* 初始高电平 (LED 灭) */
    },
    /* --- PTD15: LED0 --- */
    {
        .base          = PORTD,
        .pinPortIdx    = 15U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 1U,     /* 初始高电平 (LED 灭) */
    },
    /* --- PTD16: LED1 --- */
    {
        .base          = PORTD,
        .pinPortIdx    = 16U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_AS_GPIO,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = PTD,
        .direction     = GPIO_OUTPUT_DIRECTION,
        .digitalFilter = false,
        .initValue     = 1U,     /* 初始高电平 (LED 灭) */
    },
    /* --- PTE4: CAN0_RX (ALT5) --- */
    {
        .base          = PORTE,
        .pinPortIdx    = 4U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_ALT5,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    /* --- PTE5: CAN0_TX (ALT5) --- */
    {
        .base          = PORTE,
        .pinPortIdx    = 5U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_ALT5,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    /* --- PTA1: LPUART0_RX (ALT2) --- */
    {
        .base          = PORTA,
        .pinPortIdx    = 1U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_ALT2,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
    /* --- PTA2: LPUART0_TX (ALT2) --- */
    {
        .base          = PORTA,
        .pinPortIdx    = 2U,
        .pullConfig    = PORT_INTERNAL_PULL_NOT_ENABLED,
        .driveSelect   = PORT_LOW_DRIVE_STRENGTH,
        .passiveFilter = false,
        .mux           = PORT_MUX_ALT2,
        .pinLock       = false,
        .intConfig     = PORT_DMA_INT_DISABLED,
        .clearIntFlag  = false,
        .gpioBase      = NULL,
        .digitalFilter = false,
    },
};
