/*
 * @brief UART 模块主程序 — 串口回显
 *        对应书籍：第6章 §6.4 UART 应用实验
 *
 * 学习目标：
 *   1. 掌握 LPUART 初始化流程（时钟→波特率→数据格式）
 *   2. 实现 UART 字符收发
 *   3. 实现简单的串口回显功能
 *
 * 硬件连接：
 *   - LPUART0: PTB16(RXD) / PTB17(TXD) 通过 USB 转串口接 PC
 *   - PC 串口终端通过 USB 连接（通常为 /dev/ttyACM0 或 /dev/ttyUSB0）
 *
 * TODO 第6章学习后实现：
 *   1. 调用 uart_init() 初始化 UART
 *   2. 在主循环中实现串口回显
 *   3. 发送欢迎信息
 */
#include "uart_driver.h"

/* ========== 串口参数 ========== */
#define UART_CH          UART_CHANNEL_0
#define UART_BAUDRATE    115200
#define UART_DATA_BITS   UART_DATA_8BIT
#define UART_PARITY      UART_PARITY_NONE
#define UART_STOP_BITS   UART_STOP_1BIT

/**
 * @brief 主函数
 * @return 不会返回
 *
 * TODO §6.4：实现串口回显
 *   1. 调用 uart_init() 初始化串口
 *   2. 发送欢迎字符串 "UART Demo Ready\r\n"
 *   3. 在 while(1) 中:
 *      a. 调用 uart_getchar() 接收一个字符
 *      b. 调用 uart_putchar() 将字符回显
 *      c. 如果是 '\r' (回车)，额外发送 '\n' (换行)
 */
int main(void) {
    /* TODO：调用 uart_init() 初始化串口 */

    /* TODO：发送欢迎信息 */

    while (1) {
        /* TODO：接收字符 → 回显字符 */
    }

    return 0;
}

/* ========================================================================
 * 启动代码 (Startup Code)
 * 提示：与 gpio/ 模块共用相同的启动框架
 *       中断向量表 + Reset_Handler + 堆栈定义
 * ======================================================================== */

/* TODO：从 gpio/main.c 复制启动代码 */
/* TODO：根据需要修改栈大小 */
