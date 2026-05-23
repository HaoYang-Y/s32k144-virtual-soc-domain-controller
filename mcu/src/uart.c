/*
 * @brief UART 驱动实现 (S32K144)
 *        基于 NXP S32 SDK UART_DRV API 实现串口收发
 *
 * @note 本驱动封装 NXP S32 SDK 的 LPUART 驱动层 API
 *
 *       SDK API 涉及:
 *         - UART_DRV_Init():            初始化 UART 模块
 *         - UART_DRV_SendDataBlocking(): 阻塞发送
 *         - UART_DRV_ReceiveDataBlocking(): 阻塞接收
 *
 *       SDK 配置结构体:
 *         - uart_user_config_t:  包含波特率、数据位、校验、停止位等
 */
#include "uart.h"

/* NXP S32 SDK UART 驱动头文件 */
#include "lpuart_driver.h"

/* NXP S32 SDK 引脚复用头文件 */
#include "pins_driver.h"

/* ========== 引脚映射表 ========== */

/**
 * @brief UART 通道对应的引脚配置
 *        S32K144 EVB 默认引脚分配:
 *        - LPUART0: PTB16(RXD) / PTB17(TXD), MUX=3 (ALT3)
 *        - LPUART1: PTC6(RXD)  / PTC7(TXD),  MUX=7 (ALT7)
 *        - LPUART2: PTD2(RXD)  / PTD3(TXD),  MUX=7 (ALT7)
 */
static const uint8_t uart_rxd_pin[] = {16u, 6u, 2u};   /* RXD 引脚号 */
static const uint8_t uart_txd_pin[] = {17u, 7u, 3u};   /* TXD 引脚号 */
static const uint32_t uart_port_base[] = {
    0x4007C000u, /* PORTB 基址 */
    0x4007E000u, /* PORTC 基址 */
    0x40080000u  /* PORTD 基址 */
};

/* ========== 函数实现 ========== */

int uart_init(uart_channel_t channel, uint32_t baudrate,
              uart_data_bits_t data_bits, uart_parity_t parity,
              uart_stop_bits_t stop_bits)
{
    uart_user_config_t uartConfig;
    uint32_t port;
    uint32_t mux_val;

    /* 参数检查 */
    if ((channel > UART_CHANNEL_2) || (baudrate == 0u))
    {
        return -1;
    }

    /*
     * 步骤1: 配置引脚复用为 LPUART 功能
     * 使用 SDK 的 PINS_DRV_SetMuxModeSel() 设置 PORT PCR
     *
     * SDK API: PINS_DRV_SetMuxModeSel(portBase, pin, muxMode)
     *  - portBase: PORT 模块基地址
     *  - pin:    引脚号
     *  - muxMode: 复用功能选择 (ALT3 / ALT7)
     */
    port = uart_port_base[channel];

    /* LPUART0 使用 ALT3, LPUART1/2 使用 ALT7 */
    mux_val = (channel == 0u) ? 3u : 7u;

    /* 配置 RXD 引脚 */
    PINS_DRV_SetMuxModeSel((PORT_Type *)port,
                           uart_rxd_pin[channel],
                           mux_val);
    /* 配置 TXD 引脚 */
    PINS_DRV_SetMuxModeSel((PORT_Type *)port,
                           uart_txd_pin[channel],
                           mux_val);

    /*
     * 步骤2: 配置 UART 参数 (uart_user_config_t)
     * 使用 SDK 的默认配置作为基础
     */
    UART_DRV_GetDefaultConfig(&uartConfig);
    uartConfig.baudRate    = baudrate;
    uartConfig.bitCountPerChar = (uint8_t)data_bits;
    uartConfig.parityMode  = (uint8_t)parity;
    uartConfig.stopBitCount = (uint8_t)stop_bits;
    uartConfig.enableRx    = true;
    uartConfig.enableTx    = true;
    uartConfig.enableRxRdy = true;    /* 使能接收中断 */
    uartConfig.rxDMA       = false;   /* 不使能 DMA */
    uartConfig.txDMA       = false;

    /*
     * 步骤3: 调用 SDK API 初始化 LPUART 模块
     * SDK 内部自动完成:
     *   1. 使能 PCC_LPUARTn 时钟
     *   2. 配置 BAUD 寄存器 (SBR, OSR, SBNS)
     *   3. 配置 CTRL 寄存器 (M, PE, PT, TE, RE)
     *   4. 配置 MODIR 寄存器
     */
    UART_DRV_Init((uint32_t)channel, &uartConfig);

    return 0;
}

void uart_putchar(uart_channel_t channel, char ch)
{
    /*
     * 调用 SDK API 阻塞发送单个字符
     * SDK 内部自动处理:
     *   1. 等待 STAT[TDRE] = 1
     *   2. 写入 DATA 寄存器
     *   3. 等待发送完成
     */
    UART_DRV_SendDataBlocking((uint32_t)channel,
                              (const uint8_t *)&ch, 1u,
                              1000u);  /* 1s 超时 */
}

char uart_getchar(uart_channel_t channel)
{
    uint8_t ch;

    /*
     * 调用 SDK API 阻塞接收单个字符
     * SDK 内部自动处理:
     *   1. 等待 STAT[RDRF] = 1
     *   2. 读取 DATA 寄存器
     *   3. 检查 STAT[FE] 帧错误
     */
    UART_DRV_ReceiveDataBlocking((uint32_t)channel,
                                 &ch, 1u,
                                 1000u);  /* 1s 超时 */

    return (char)ch;
}

void uart_puts(uart_channel_t channel, const char *str)
{
    const char *p = str;

    /* 逐字符发送至字符串结束 */
    while (*p != '\0')
    {
        uart_putchar(channel, *p);
        p++;
    }

    /* 追加换行 */
    uart_putchar(channel, '\r');
    uart_putchar(channel, '\n');
}
