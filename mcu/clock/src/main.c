/*
 * @brief Clock 模块主程序 — 系统时钟配置验证
 *        对应书籍：第5章 时钟系统实验
 *
 * 学习目标：
 *   1. 理解 S32K144 时钟树结构
 *   2. 掌握 SCG 寄存器配置流程
 *   3. 实现外部晶振和 PLL 的配置
 *   4. 验证各时钟源切换和频率输出
 *
 * TODO 第5章学习后实现：
 *   1. clock_init_sosc() 初始化外部晶振
 *   2. clock_init_spll() 初始化 PLL 到 160MHz
 *   3. clock_set_sysclk() 切换到 SPLL
 *   4. clock_get_freq() 验证系统频率
 *   5. 通过切换不同时钟源观察 LED 闪烁频率变化
 */
#include "clock_driver.h"

/* ========== 引脚定义 ========== */
/* 使用 PTC3/SPI1_PCS1 或 PTA3 输出时钟信号 */
#define CLKOUT_PIN          3
#define CLKOUT_PIN_PORT     3  /* PTC3 */

/**
 * @brief 主函数
 * @return 不会返回
 *
 * TODO §5.3：实现时钟配置验证
 *   1. 配置默认时钟 (FIRC)
 *   2. 初始化 UART
 *   3. 打印当前频率
 *   4. 初始化 SOSC
 *   5. 切换到 SOSC，打印频率
 *   6. 初始化 SPLL
 *   7. 切换到 SPLL，打印频率
 *   8. while(1) 循环等待
 */
int main(void) {
    /* TODO：获取并打印当前默认频率 (FIRC 48MHz) */

    /* TODO：初始化外部晶振 SOSC 8MHz */

    /* TODO：切换到 SOSC 并验证频率 */

    /* TODO：初始化 SPLL 160MHz */

    /* TODO：切换到 SPLL 并验证频率 */

    while (1) {
        /* TODO：主循环保持时钟运行 */
    }

    return 0;
}

/* ========================================================================
 * 启动代码 (Startup Code)
 * 提示：与 gpio/ 模块共用相同的启动框架
 * ======================================================================== */

/* TODO：从 gpio/main.c 复制启动代码 */
