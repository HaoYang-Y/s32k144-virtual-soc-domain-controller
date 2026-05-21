/*
 * @brief GPIO 模块主程序 — LED 闪烁
 *        对应书籍：第4章 §4.4 利用构件方法控制LED闪烁
 *
 * 学习目标：
 *   1. 理解 MCU 启动流程（Reset_Handler → main）
 *   2. 掌握 GPIO 寄存器配置（方向/输出）
 *   3. 实现第一个 LED 闪烁程序
 *
 * 硬件连接：
 *   - PTD15: 板载 LED（S32K144EVB 蓝色 LED）
 *
 * TODO 第4章学习后实现：
 *   1. 补全中断向量表和 Reset_Handler
 *   2. 调用 gpio_init() 配置 LED 引脚
 *   3. 在 main 循环中控制 LED 闪烁 (500ms 周期)
 */
#include "gpio_driver.h"

/* ========== 引脚定义 ========== */
#define LED_PORT    GPIO_PORT_D
#define LED_PIN     15

/**
 * @brief 系统初始化
 *        配置板级基本外设
 *
 * TODO §4.3：实现 board_init()
 *   1. 调用 gpio_init() 初始化 LED 引脚为输出模式
 *   2. 初始状态 LED 熄灭
 */
static void board_init(void) {
    /* TODO：初始化 LED 引脚为推挽输出 */

    /* TODO：初始状态 LED 熄灭 */
}

/**
 * @brief 主函数
 * @return 不会返回
 *
 * TODO §4.4：实现主循环
 *   1. 调用 board_init()
 *   2. 在 while(1) 中:
 *      a. gpio_toggle(LED_PORT, LED_PIN) 翻转 LED
 *      b. gpio_delay_ms(500) 等待 500ms
 */
int main(void) {
    /* TODO：调用 board_init() */

    /* TODO：实现主循环 */
    while (1) {
        /* TODO：翻转 LED */

        /* TODO：延迟 500ms */
    }

    return 0;
}

/* ========================================================================
 * 启动代码 (Startup Code)
 * 对应书籍：§4.5 工程文件组织框架
 *
 * 在实现 main() 之后，还需要补全以下内容：
 *   1. 中断向量表 (.isr_vector 段) — §4.5.4
 *      - 栈顶地址
 *      - Reset_Handler
 *      - 其他异常向量 (HardFault, SVCall, SysTick 等)
 *   2. Reset_Handler — §4.5.2
 *      - 复制 .data 段到 RAM
 *      - 清零 .bss 段
 *      - 调用 main()
 *   3. 堆栈定义
 * ======================================================================== */
