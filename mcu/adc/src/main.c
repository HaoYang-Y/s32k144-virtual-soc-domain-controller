/*
 * @brief ADC 模块主程序 — 电位器电压采集
 *        对应书籍：第10章 ADC 应用实验
 *
 * 学习目标：
 *   1. 掌握 ADC 初始化流程（时钟→分辨率→采样时间）
 *   2. 实现单次 ADC 转换
 *   3. 将 ADC 原始值转换为实际电压
 *
 * 硬件连接：
 *   - 电位器输出接 ADC0 通道 (如 PTB0/ADC0_SE8)
 *   - LED 用于指示采样状态
 *
 * TODO 第10章学习后实现：
 *   1. 调用 adc_init() 初始化 ADC
 *   2. 在主循环中读取电位器电压
 *   3. LED 亮度随电压变化（PWM 需配合 FTM 模块）
 */
#include "adc_driver.h"

/* ========== 引脚定义 ========== */
/* 提示：参考原理图确认 ADC 输入引脚 */
#define ADC_INPUT_CHANNEL   ADC_CHANNEL_8   /* PTB0/ADC0_SE8 */

/**
 * @brief 主函数
 * @return 不会返回
 *
 * TODO §10.3：实现 ADC 电压采集
 *   1. 初始化 ADC (12位分辨率，VREF)
 *   2. 初始化 UART（用于打印电压值）
 *   3. 在 while(1) 中:
 *      a. adc_read() 读取 ADC 值
 *      b. adc_raw_to_mv() 转为电压
 *      c. UART 打印: "Voltage: %d mV\r\n"
 *      d. 适当延时 (systick_delay_ms 或简单循环)
 */
int main(void) {
    /* TODO：调用 adc_init() 初始化 ADC */

    /* TODO：初始化 UART 用于输出 */

    while (1) {
        /* TODO：读取 ADC 值并打印 */
    }

    return 0;
}

/* ========================================================================
 * 启动代码 (Startup Code)
 * 提示：与 gpio/ 模块共用相同的启动框架
 * ======================================================================== */

/* TODO：从 gpio/main.c 复制启动代码 */
