/**
 * @file    pin_mux.h
 * @brief   引脚复用配置 — CAN0 + LED + LPUART0
 */

#ifndef PIN_MUX_H
#define PIN_MUX_H

#include "pins_driver.h"

/* CAN 引脚定义 */
#define CAN0_RX_PORT  PTE
#define CAN0_RX_PIN   4U
#define CAN0_TX_PORT  PTE
#define CAN0_TX_PIN   5U

/* LED 引脚定义 */
#define LED_BLUE_PORT  PTD
#define LED_BLUE_PIN   0U
#define LED_PTD1_PORT  PTD
#define LED_PTD1_PIN   1U
#define LED_RED_PORT   PTD
#define LED_RED_PIN    15U
#define LED_GREEN_PORT PTD
#define LED_GREEN_PIN  16U

/* 向后兼容 */
#define LED0_PORT     PTD
#define LED0_PIN      15U
#define LED1_PORT     PTD
#define LED1_PIN      16U

/* LPUART0 引脚定义 (PTA1=RX, PTA2=TX, ALT2) */
#define UART0_RX_PORT  PTA
#define UART0_RX_PIN   1U
#define UART0_TX_PORT  PTA
#define UART0_TX_PIN   2U

/* LPSPI1 Slave 引脚定义 (ALT3) — Arduino SPI 排母 */
#define LPSPI1_SCK_PORT   PTB
#define LPSPI1_SCK_PIN    14U
#define LPSPI1_SIN_PORT   PTB
#define LPSPI1_SIN_PIN    16U
#define LPSPI1_SOUT_PORT  PTB
#define LPSPI1_SOUT_PIN   15U
#define LPSPI1_PCS3_PORT  PTB
#define LPSPI1_PCS3_PIN   17U

/* 已配置引脚总数 */
#define NUM_OF_CONFIGURED_PINS0  12U

/* 引脚配置数组（在 pin_mux.c 中定义） */
extern pin_settings_config_t g_pin_mux_InitConfigArr0[NUM_OF_CONFIGURED_PINS0];

#endif /* PIN_MUX_H */
