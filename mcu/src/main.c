/**
 * @brief AUTOSAR CP MCAL 层裸机应用入口
 *
 * 演示 AUTOSAR CP 中 MCAL 驱动程序的标准使用方式：
 *   1. Mcu 驱动初始化系统时钟
 *   2. Gpio 驱动初始化 IO 引脚
 *   3. 主循环中通过 MCAL 接口操作 GPIO
 *
 * @note S32K144-EVB 蓝色 LED 连接在 PTD15，低电平点亮
 */

#include "McuDrv.h"
#include "GpioDrv.h"

/* ========== GPIO 通道配置 ========== */

/** @brief LED 通道配置表 (PTD15 = 蓝色 LED, 输出, 无上下拉) */
static const GpioDrv_ChannelCfgType channel_cfgs[] = {
    {
        .channel   = GPIO_CH(3, 15),  /**< PTD15: 端口 D 引脚 15 (蓝色 LED) */
        .direction = GPIO_CFG_DIR_OUTPUT,
        .pull      = GPIO_CFG_PULL_DISABLE,
    },
};

/** @brief GPIO 驱动总配置 */
static const GpioDrv_ConfigType gpio_cfg = {
    .num_channels = 1,
    .channels     = channel_cfgs,
};

/* ========== 简易延时 ========== */

/**
 * @brief 简易软件延时（阻塞）
 *
 * @param[in]  cycles  延时循环次数，约为 cycles × 若干指令周期
 *
 * @note 实测 FIRC 48MHz 下，cycles=2000000 约延时 500ms
 */
static void delay_loop(volatile uint32_t cycles)
{
    while (cycles > 0) {
        cycles--;
    }
}

/* ========== 入口函数 ========== */

/**
 * @brief C 程序主入口
 *
 * 在 AUTOSAR CP 裸机环境中：
 * - 上电后由 startup_S32K144.S 的 Reset_Handler 调用
 * - 完成 MCAL 各驱动初始化后进入应用主循环
 *
 * @return 无返回值（裸机程序永不退出）
 */
int main(void)
{
    /* —— Step 1: 初始化 MCU 时钟 （MCAL Mcu 驱动） —— */
    /* 使用默认 FIRC 48MHz, Core/Bus 均运行在 48MHz */
    Mcu_InitClock(NULL);

    /* —— Step 2: 初始化 GPIO 驱动 （MCAL Gpio 驱动） —— */
    /* 配置 PTD15 为输出模式 */
    GpioDrv_Init(&gpio_cfg);

    /* —— Step 3: 主循环 —— */
    /* 通过 MCAL 接口控制 GPIO，实现 LED 闪烁 */
    while (1) {
        /* 点亮 LED (PTD15 低电平点亮) */
        Gpio_WritePin(GPIO_CH(3, 15), false);
        delay_loop(2000000);

        /* 熄灭 LED (PTD15 高电平熄灭) */
        Gpio_WritePin(GPIO_CH(3, 15), true);
        delay_loop(2000000);
    }
}
