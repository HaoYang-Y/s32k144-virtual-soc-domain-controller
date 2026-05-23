/*
 * @brief UART 驱动实现 (S32K144)
 *        通过直接操作寄存器实现 LPUART 串口收发
 *
 * @note 参考: S32K1xx Reference Manual, Chapter 53 (LPUART)
 *       对应书籍：第6章 串口通信
 *
 * TODO：阅读 §6.2 后，按以下步骤实现每个函数
 *       关键寄存器：
 *         - BAUD:  波特率控制 (SBR[12:0], OSR[4:0])
 *         - STAT:  状态寄存器 (TDRE, RDRF, FE, NF)
 *         - CTRL:  控制寄存器 (TE, RE, PE, PT, M)
 *         - DATA:  数据寄存器
 *         - MODIR: 调制控制
 *         - GLOBAL: 全局控制 (RST)
 *         - FILT:  滤波控制
 */
#include "uart.h"

/* ========== 寄存器结构体定义 ========== */

/*
 * TODO §6.2.2：定义 LPUART 寄存器结构体
 * 提示：包含 BAUD / STAT / CTRL / DATA / MODIR / GLOBAL / ...
 * 参考手册 LPUART 寄存器列表
 */
typedef struct {
    /* LPUART 寄存器映射 */
} lpuart_regs_t;

/* ========== 寄存器基址映射 ========== */

/* TODO：将 3 个 LPUART 通道的基地址映射为 lpuart_regs_t* 指针数组 */
/* TODO：LPUART0/LPUART1/LPUART2 的 PCI 功能选择（PCR MUX）引脚号 */
/* 提示：通常 LPUART0 使用 PTB16(RXD)/PTB17(TXD)，MUX=3 */
/* 提示：先查开发板原理图确认具体引脚 */

/* ========== 函数实现 ========== */

int uart_init(uart_channel_t channel, uint32_t baudrate,
              uart_data_bits_t data_bits, uart_parity_t parity,
              uart_stop_bits_t stop_bits) {
    /*
     * TODO §6.2.3：实现 UART 初始化
     * 步骤：
     *   1. 参数检查
     *   2. 使能 LPUART 时钟 (PCC_LPUARTn[CGC])
     *   3. LPUART 软件复位: GLOBAL[RST] = 1, 然后清 0
     *   4. 配置波特率 BAUD 寄存器:
     *      - SBR[12:0] = 时钟 / (OSR * 波特率)
     *      - OSR[4:0]  = 15 (过采样率)
     *   5. 配置 CTRL 寄存器:
     *      - M:  数据位 (8 位/7 位)
     *      - PE: 校验使能
     *      - PT: 校验类型 (偶/奇)
     *   6. 配置停止位: BAUD[SBNS] = 1 表示 2 位停止位
     *   7. 使能发送器 CTRL[TE] = 1
     *   8. 使能接收器 CTRL[RE] = 1
     *   9. 配置引脚复用为 LPUART 功能 (PORT PCR)
     *   10. 返回 0
     */
    (void)channel;
    (void)baudrate;
    (void)data_bits;
    (void)parity;
    (void)stop_bits;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return 0;
}

void uart_putchar(uart_channel_t channel, char ch) {
    /*
     * TODO §6.2.3：实现 UART 发送
     * 步骤：
     *   1. 等待 STAT[TDRE] = 1 (发送数据寄存器空)
     *   2. 写入 DATA 寄存器 (ch)
     * 提示：TDRE 是发送缓冲区可写标志
     */
    (void)channel;
    (void)ch;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}

char uart_getchar(uart_channel_t channel) {
    /*
     * TODO §6.2.3：实现 UART 接收
     * 步骤：
     *   1. 等待 STAT[RDRF] = 1 (接收数据寄存器满)
     *   2. 读取 DATA 寄存器
     * 提示：RDRF 是接收缓冲区有数据标志
     *       可以先检查 STAT[FE] 帧错误
     */
    (void)channel;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return 0;
}

void uart_puts(uart_channel_t channel, const char *str) {
    /*
     * TODO §6.2.3：实现 UART 字符串发送
     * 提示：循环调用 uart_putchar() 直到 str[i] == '\0'
     *       可以在末尾追加 "\r\n" 实现换行
     */
    (void)channel;
    (void)str;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}
