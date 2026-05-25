/*
 * @brief S32K144 域控制器 MCU 端主程序
 *
 * @note 最小功能验证版本:
 *       1. 初始化 GPIO (PTD15 红色LED, PTD16 绿色LED)
 *       2. 循环交替闪烁两个 LED
 *
 *       S32K144-EVB 开发板 LED 引脚 (实际连线):
 *         - 红色 LED (LED4): PTD15
 *         - 绿色 LED (LED5): PTD16
 */
#include "gpio.h"

/** @brief LED 引脚定义 (S32K144-EVB) */
#define LED_RED_PORT     GPIO_PORT_D
#define LED_RED_PIN      15u
#define LED_GREEN_PORT   GPIO_PORT_D
#define LED_GREEN_PIN    16u

/** @brief LED 闪烁间隔 (毫秒) */
#define BLINK_DELAY_MS   500u

/**
 * @brief 系统初始化
 *
 * 初始化 GPIO 作为 LED 输出
 *
 * @return 0=成功
 */
static int system_init(void)
{
    /* 初始化红色 LED 引脚: PTD15, 输出, 无上下拉 */
    int ret = gpio_init(LED_RED_PORT, LED_RED_PIN,
                        GPIO_DIR_OUTPUT, GPIO_PULL_DISABLE);
    if (ret != 0)
    {
        return -1;
    }

    /* 初始化绿色 LED 引脚: PTD16, 输出, 无上下拉 */
    ret = gpio_init(LED_GREEN_PORT, LED_GREEN_PIN,
                    GPIO_DIR_OUTPUT, GPIO_PULL_DISABLE);
    if (ret != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief 主函数
 *
 * 循环交替闪烁两个 LED:
 *   - 绿色LED亮 + 蓝色LED灭 → 延时 500ms
 *   - 绿色LED灭 + 蓝色LED亮 → 延时 500ms
 *
 * @return 永不返回 (死循环)
 */
int main(void)
{
    int      ret;
    uint32_t i;

    ret = system_init();

    if (ret != 0)
    {
        /* 初始化失败: 红色 LED 慢闪 1Hz 错误提示 */
        while (1)
        {
            gpio_write(LED_RED_PORT, LED_RED_PIN, true);
            gpio_delay_ms(500u);
            gpio_write(LED_RED_PORT, LED_RED_PIN, false);
            gpio_delay_ms(500u);
        }
    }

    /* 初始化成功: 红灯快闪 3 次，确认程序运行 */
    for (i = 0; i < 3u; i++)
    {
        gpio_write(LED_RED_PORT, LED_RED_PIN, true);
        gpio_delay_ms(BLINK_DELAY_MS / 3u);
        gpio_write(LED_RED_PORT, LED_RED_PIN, false);
        gpio_delay_ms(BLINK_DELAY_MS / 3u);
    }

    /* 主循环: 两个 LED 交替闪烁 */
    while (1)
    {
        gpio_write(LED_RED_PORT, LED_RED_PIN, true);
        gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, false);
        gpio_delay_ms(BLINK_DELAY_MS);

        gpio_write(LED_RED_PORT, LED_RED_PIN, false);
        gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, true);
        gpio_delay_ms(BLINK_DELAY_MS);
    }

    return 0;
}
