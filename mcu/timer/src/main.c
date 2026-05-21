/*
 * @brief 定时器模块主程序 — SysTick 精确延时 + LPIT 定时
 *        对应书籍：第7章 §7.3~§7.4
 *
 * 学习目标：
 *   1. 掌握 SysTick 定时器配置（内核定时器）
 *   2. 掌握 LPIT 定时器配置（外设定时器）
 *   3. 实现精确毫秒级延时
 *   4. 理解定时器中断处理流程
 *
 * TODO 第7章学习后实现：
 *   1. 调用 systick_init() 初始化系统心跳
 *   2. 使用 systick_delay_ms() 控制 LED 精确闪烁
 *   3. (进阶) 配置 LPIT 通道实现微秒级定时
 */
#include "timer_driver.h"

/* ========== 引脚定义（复用 gpio 驱动） ========== */
/* 提示：需要包含 gpio_driver.h 来控制 LED */
#define LED_PORT    GPIO_PORT_D
#define LED_PIN     15

/**
 * @brief 主函数
 * @return 不会返回
 *
 * TODO §7.3.2：实现 SysTick 精确延时控制 LED 闪烁
 *   1. 初始化 GPIO（LED 引脚为输出）
 *   2. 调用 systick_init(48000000) 初始化 SysTick
 *   3. 在 while(1) 中:
 *      a. GPIO 翻转 LED
 *      b. systick_delay_ms(500) 精确延时 500ms
 *
 * TODO §7.4 (进阶)：使用 LPIT 替代 GPIO 延迟
 *   1. 配置 LPIT 通道为 100ms 周期
 *   2. 在主循环中查询 lpit_is_timeout()
 *   3. 超时则翻转 LED
 */
int main(void) {
    /* TODO：初始化 LED 引脚为输出 */

    /* TODO：初始化 SysTick（CPU=48MHz） */
    /* systick_init(48000000); */

    /* TODO：初始化 LPIT 通道（可选） */

    while (1) {
        /*
         * TODO：主循环
         * 方式 A：用 systick_delay_ms(500) 延时
         * 方式 B：查询 LPIT 超时标志，超时则翻转并重启
         */
    }

    return 0;
}

/* ========================================================================
 * 启动代码 (Startup Code)
 * 提示：与 gpio/ 模块共用相同的启动框架
 *       中断向量表 + Reset_Handler + 堆栈定义
 *       注意：中断向量表需要包含 SysTick_Handler 的入口
 * ======================================================================== */

/* TODO：从 gpio/main.c 复制启动代码 */
/* TODO：添加 SysTick_Handler 到中断向量表 */
