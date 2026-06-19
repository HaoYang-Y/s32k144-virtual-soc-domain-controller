/**
 * @file    Uart_Cfg.h
 * @brief   CDD Uart 预编译配置
 */

#ifndef UART_CFG_H
#define UART_CFG_H

/** @brief 默认波特率 */
#define UART_DEFAULT_BAUDRATE   115200UL

/** @brief 默认超时 (ms) */
#define UART_DEFAULT_TIMEOUT_MS 1000UL

/** @brief LPUART 实例号 (固定 LPUART0) */
#define UART_INSTANCE           0U

/** @brief 数据位 (8N1) */
#define UART_DATA_BITS          8U

#endif /* UART_CFG_H */
