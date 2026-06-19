/**
 * @file    Uart.c
 * @brief   CDD Uart 模块实现 — LPUART0 Polling 模式
 *
 * @note    SDK 依赖全部封装在本 .c 内部，上层代码无需包含 lpuart_driver.h
 */

#include "Uart.h"
#include "lpuart_driver.h"
#include <string.h>

/* ===================================================================
 *  模块私有状态
 * =================================================================== */

static lpuart_state_t        uart_state;    /**< SDK 驱动状态          */
static lpuart_user_config_t  uart_config;   /**< SDK 驱动配置          */
static bool                  uart_inited = false;

/* ===================================================================
 *  API 实现
 * =================================================================== */

status_t Uart_Init(uint32_t baudRate)
{
    if (uart_inited) {
        return STATUS_SUCCESS;  /* 已初始化 */
    }

    /* 获取 SDK 默认配置，然后覆盖波特率 */
    LPUART_DRV_GetDefaultConfig(&uart_config);
    uart_config.baudRate        = baudRate;
    uart_config.parityMode      = LPUART_PARITY_DISABLED;
    uart_config.stopBitCount    = LPUART_ONE_STOP_BIT;
    uart_config.bitCountPerChar = LPUART_8_BITS_PER_CHAR;

    /* Polling 模式: 不依赖中断，不依赖 DMA */
    uart_config.transferType = LPUART_USING_INTERRUPTS; /* 仍需传给 SDK，但我们只用 polling API */

    status_t ret = LPUART_DRV_Init(UART_INSTANCE, &uart_state, &uart_config);
    if (ret == STATUS_SUCCESS) {
        uart_inited = true;
    }
    return ret;
}

status_t Uart_DeInit(void)
{
    if (!uart_inited) {
        return STATUS_SUCCESS;
    }
    status_t ret = LPUART_DRV_Deinit(UART_INSTANCE);
    if (ret == STATUS_SUCCESS) {
        uart_inited = false;
    }
    return ret;
}

status_t Uart_SendData(const uint8_t *data, uint32_t length, uint32_t timeoutMs)
{
    if (!uart_inited || (data == NULL) || (length == 0U)) {
        return STATUS_ERROR;
    }

    return LPUART_DRV_SendDataBlocking(UART_INSTANCE, data, length, timeoutMs);
}

status_t Uart_SendString(const char *str)
{
    if (!uart_inited || (str == NULL)) {
        return STATUS_ERROR;
    }

    uint32_t len = (uint32_t)strlen(str);
    if (len == 0U) {
        return STATUS_SUCCESS;
    }

    return LPUART_DRV_SendDataBlocking(UART_INSTANCE,
                                       (const uint8_t *)str,
                                       len,
                                       UART_DEFAULT_TIMEOUT_MS);
}
