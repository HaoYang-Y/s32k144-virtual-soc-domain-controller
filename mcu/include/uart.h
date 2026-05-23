/*
 * @brief UART 驱动头文件 (S32K144)
 *        封装基于 NXP S32 SDK 的 LPUART 串口收发功能
 *
 * @note 本驱动使用 NXP S32 SDK LPUART_DRV API 实现
 *       涉及的头文件：lpuart_driver.h
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== 枚举定义 ========== */

/** @brief UART 通道枚举 */
typedef enum {
    UART_CHANNEL_0 = 0,
    UART_CHANNEL_1 = 1,
    UART_CHANNEL_2 = 2,
} uart_channel_t;

/** @brief 数据位长度 */
typedef enum {
    UART_DATA_8BIT = 0,     /**< 8 位数据位 */
    UART_DATA_7BIT = 1,     /**< 7 位数据位 */
} uart_data_bits_t;

/** @brief 校验模式 */
typedef enum {
    UART_PARITY_NONE = 0,   /**< 无校验 */
    UART_PARITY_EVEN = 1,   /**< 偶校验 */
    UART_PARITY_ODD  = 2,   /**< 奇校验 */
} uart_parity_t;

/** @brief 停止位 */
typedef enum {
    UART_STOP_1BIT = 0,     /**< 1 位停止位 */
    UART_STOP_2BIT = 1,     /**< 2 位停止位 */
} uart_stop_bits_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化 UART
 *        通过 NXP S32 SDK 配置 LPUART 时钟、波特率、数据格式
 *
 * @param[in]  channel     UART 通道 (0/1/2)
 * @param[in]  baudrate    波特率 (9600/115200/...)
 * @param[in]  data_bits   数据位 (7/8)
 * @param[in]  parity      校验模式
 * @param[in]  stop_bits   停止位 (1/2)
 * @return     0=成功, -1=参数错误
 *
 * @note SDK API: LPUART_DRV_Init()
 */
int uart_init(uart_channel_t channel, uint32_t baudrate,
              uart_data_bits_t data_bits, uart_parity_t parity,
              uart_stop_bits_t stop_bits);

/**
 * @brief 发送单个字符（阻塞）
 *
 * @param[in]  channel   UART 通道
 * @param[in]  ch        待发送的字符
 *
 * @note SDK API: LPUART_DRV_SendDataBlocking()
 */
void uart_putchar(uart_channel_t channel, char ch);

/**
 * @brief 接收单个字符（阻塞）
 *
 * @param[in]  channel   UART 通道
 * @return     接收到的字符
 *
 * @note SDK API: LPUART_DRV_ReceiveDataBlocking()
 */
char uart_getchar(uart_channel_t channel);

/**
 * @brief 发送字符串（阻塞）
 *
 * @param[in]  channel   UART 通道
 * @param[in]  str       以 '\0' 结尾的字符串
 *
 * @note 循环调用 uart_putchar() 直到遇到 '\0'
 */
void uart_puts(uart_channel_t channel, const char *str);
