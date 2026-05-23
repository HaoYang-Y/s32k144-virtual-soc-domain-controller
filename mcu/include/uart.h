/*
 * @brief UART 驱动头文件 (S32K144)
 *        封装 UART 模块的寄存器操作，提供串口收发功能
 *
 * @note 学习目标：理解 UART 异步串行通信协议及寄存器配置
 *       对应书籍：第6章 串口通信 (§6.2~§6.4)
 *
 * TODO：看书后实现以下函数
 *       1. uart_init()     — 初始化 UART 波特率、数据格式
 *       2. uart_putchar()  — 发送单个字符
 *       3. uart_getchar()  — 接收单个字符（阻塞）
 *       4. uart_puts()     — 发送字符串
 *       5. uart_printf()   — 格式化输出（可选）
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== UART 基地址 ========== */
/* TODO §6.2：从 S32K144 参考手册中找到 LPUART 寄存器基地址 */
#define LPUART0_BASE    (0x4006A000u)
#define LPUART1_BASE    (0x4006B000u)
#define LPUART2_BASE    (0x4006C000u)

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
 *        1. 使能 LPUART 时钟 (PCC)
 *        2. 配置引脚复用为 UART 功能 (PORT PCR)
 *        3. 复位 LPUART (GLOBAL 寄存器)
 *        4. 配置波特率 (BAUD 寄存器)
 *        5. 配置数据格式 (CTRL: M/PE/PT 位)
 *        6. 使能发送器和接收器 (CTRL: TE/RE 位)
 *
 * @param[in]  channel     UART 通道 (0/1/2)
 * @param[in]  baudrate    波特率 (9600/115200/...)
 * @param[in]  data_bits   数据位 (7/8)
 * @param[in]  parity      校验模式
 * @param[in]  stop_bits   停止位 (1/2)
 * @return     0=成功, -1=参数错误
 *
 * TODO：阅读 §6.2.2 LPUART 寄存器描述，实现此函数
 */
int uart_init(uart_channel_t channel, uint32_t baudrate,
              uart_data_bits_t data_bits, uart_parity_t parity,
              uart_stop_bits_t stop_bits);

/**
 * @brief 发送单个字符（阻塞）
 * @param[in]  channel   UART 通道
 * @param[in]  ch        待发送的字符
 *
 * TODO：等待 TDRE 标志置位 → 写入 DATA 寄存器
 *       提示：检查 STAT[TDRE] 位
 */
void uart_putchar(uart_channel_t channel, char ch);

/**
 * @brief 接收单个字符（阻塞）
 * @param[in]  channel   UART 通道
 * @return     接收到的字符
 *
 * TODO：等待 RDRF 标志置位 → 读取 DATA 寄存器
 *       提示：检查 STAT[RDRF] 位
 */
char uart_getchar(uart_channel_t channel);

/**
 * @brief 发送字符串（阻塞）
 * @param[in]  channel   UART 通道
 * @param[in]  str       以 '\0' 结尾的字符串
 *
 * TODO：循环调用 uart_putchar() 直到遇到 '\0'
 */
void uart_puts(uart_channel_t channel, const char *str);
