/**
 * @brief S32K144 LED常亮验证 — 使用驱动库 API 版
 *        调用 gpio_driver.h 的 gpio_init / gpio_write 函数
 *
 * @note  S32K144EVB-Q100 板载 LED 映射：
 *         - PTD0  → RGB 蓝色 LED (SW1-1 使能)
 *         - PTD1  → 通用 LED        (SW1-2 使能)
 *         - PTD15 → RGB 红色 LED   (SW1-3 使能)
 *         - PTD16 → RGB 绿色 LED   (SW1-4 使能)
 *
 * @note  LED 为共阳极（Active LOW），输出低电平点亮
 * @note  SW1 DIP 开关必须全部拨到 ON 位置
 */
#include "gpio_driver.h"

/**
 * @brief LED 引脚配置表
 *        (端口, 引脚, 方向, 上下拉)
 */
static const gpio_port_t LED_PORT[] = {
    GPIO_PORT_D,  /* PTD0 */
    GPIO_PORT_D,  /* PTD1 */
    GPIO_PORT_D,  /* PTD15 */
    GPIO_PORT_D,  /* PTD16 */
};

static const uint8_t LED_PIN[] = {
    0,   /* PTD0 - RGB 蓝 */
    1,   /* PTD1 - 通用 */
    15,  /* PTD15 - RGB 红 */
    16,  /* PTD16 - RGB 绿 */
};

#define LED_COUNT  (sizeof(LED_PIN) / sizeof(LED_PIN[0]))

/**
 * @brief 初始化所有 LED 引脚并点亮
 *        调用 gpio_driver 库函数实现
 *
 * @note 包括：
 *        1. 使能 PORTD 时钟 (PCC)
 *        2. 配置 PORT PCR (MUX=GPIO, 无上下拉)
 *        3. 配置 GPIO PDDR (方向=输出)
 *        4. 输出低电平 (PCOR 写 1)
 *
 * @return 0=成功, -1=失败
 */
static int led_all_on(void)
{
    int ret;

    for (uint32_t i = 0u; i < LED_COUNT; i++)
    {
        /* 初始化引脚：输出模式，无上下拉 */
        ret = gpio_init(LED_PORT[i], LED_PIN[i],
                        GPIO_DIR_OUTPUT, GPIO_PULL_DISABLE);
        if (ret != 0)
        {
            return -1;
        }

        /* 写 1 点亮 LED（对应 S32K144EVB 官方 SDK 默认行为）*/
        gpio_write(LED_PORT[i], LED_PIN[i], true);
    }

    return 0;
}

/**
 * @brief 主函数
 *        初始化所有 LED 并保持常亮
 *
 * @return 不应返回
 */
int main(void)
{
    /* 初始化并点亮所有 LED */
    (void)led_all_on();

    /* 死循环保持 LED 常亮 */
    while (1u)
    {
        /* 空操作，编译器不会优化掉 */
        __asm__ volatile ("nop");
    }
}
