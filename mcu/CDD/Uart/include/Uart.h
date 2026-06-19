/**
 * @file    Uart.h
 * @brief   CDD Uart 模块 — LPUART0 驱动封装 (AUTOSAR CDD 层)
 *
 * @note    封装 NXP SDK lpuart_driver，对外暴露干净的 AUTOSAR 风格接口。
 *          - 使用 LPUART0，Polling 模式（不依赖中断）
 *          - 时钟 LPUART0_CLK 已在 clock_config.c 中使能 (8MHz SOSC_DIV2)
 *          - 引脚 PTA1 (RX) / PTA2 (TX) 在 pin_mux.c 中配置
 *
 * 用法:
 *   Uart_Init(115200);                   // 初始化
 *   Uart_SendString("hello\n");          // 发送字符串
 *   Uart_SendData(data_buf, len, 100);   // 发送字节数组（阻塞，超时 100ms）
 */

#ifndef UART_H
#define UART_H

#include "Std_Types.h"
#include "status.h"

/* ===================================================================
 *  配置常量 (来自 Uart_Cfg.h)
 * =================================================================== */
#include "Uart_Cfg.h"

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 初始化 LPUART0
 * @param baudRate  波特率 (e.g. 115200, 230400, 460800, 921600)
 * @return STATUS_SUCCESS / STATUS_ERROR
 */
status_t Uart_Init(uint32_t baudRate);

/**
 * @brief 反初始化 LPUART0
 * @return STATUS_SUCCESS / STATUS_ERROR
 */
status_t Uart_DeInit(void);

/**
 * @brief 阻塞发送字节数组
 * @param data      数据指针
 * @param length    数据长度
 * @param timeoutMs 超时毫秒数
 * @return STATUS_SUCCESS / STATUS_TIMEOUT / STATUS_ERROR
 */
status_t Uart_SendData(const uint8_t *data, uint32_t length, uint32_t timeoutMs);

/**
 * @brief 发送以 '\0' 结尾的字符串（阻塞，内部用 SendData）
 * @param str  字符串指针
 * @return STATUS_SUCCESS / STATUS_ERROR
 */
status_t Uart_SendString(const char *str);

#endif /* UART_H */
