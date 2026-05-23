/*
 * @brief GPIO 驱动头文件 (S32K144)
 *        封装基于 NXP S32 SDK 的 GPIO 引脚控制功能
 *
 * @note 本驱动使用 NXP S32 SDK PINS_DRV / GPIO_DRV API 实现
 *       涉及的头文件：pins_driver.h, gpio_driver.h
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== 枚举定义 ========== */

/** @brief GPIO 端口枚举 */
typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B = 1,
    GPIO_PORT_C = 2,
    GPIO_PORT_D = 3,
} gpio_port_t;

/** @brief GPIO 方向 */
typedef enum {
    GPIO_DIR_INPUT  = 0,    /**< 输入模式 */
    GPIO_DIR_OUTPUT = 1,    /**< 输出模式 */
} gpio_direction_t;

/** @brief GPIO 上拉/下拉配置 */
typedef enum {
    GPIO_PULL_DISABLE = 0,  /**< 无上拉/下拉 */
    GPIO_PULL_DOWN    = 1,  /**< 下拉 */
    GPIO_PULL_UP      = 2,  /**< 上拉 */
} gpio_pull_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化 GPIO 引脚
 *        通过 NXP S32 SDK 使能 PORT 时钟，配置 PCR 复用功能和方向
 *
 * @param[in]  port   端口 (A/B/C/D)
 * @param[in]  pin    引脚号 (0~31)
 * @param[in]  dir    方向 (输入/输出)
 * @param[in]  pull   上拉/下拉配置
 * @return     0=成功, -1=参数错误
 *
 * @note SDK API: PINS_DRV_Init() / PINS_DRV_SetPinsDirection()
 */
int gpio_init(gpio_port_t port, uint8_t pin,
              gpio_direction_t dir, gpio_pull_t pull);

/**
 * @brief 设置 GPIO 引脚输出电平
 *
 * @param[in]  port   端口
 * @param[in]  pin    引脚号
 * @param[in]  value  true=高电平, false=低电平
 *
 * @note SDK API: GPIO_DRV_SetPinsOutput() / GPIO_DRV_ClearPinsOutput()
 */
void gpio_write(gpio_port_t port, uint8_t pin, bool value);

/**
 * @brief 读取 GPIO 引脚输入电平
 *
 * @param[in]  port   端口
 * @param[in]  pin    引脚号
 * @return     true=高电平, false=低电平
 *
 * @note SDK API: GPIO_DRV_ReadPinsInput()
 */
bool gpio_read(gpio_port_t port, uint8_t pin);

/**
 * @brief 翻转 GPIO 引脚输出电平
 *
 * @param[in]  port   端口
 * @param[in]  pin    引脚号
 *
 * @note SDK API: GPIO_DRV_TogglePinsOutput()
 */
void gpio_toggle(gpio_port_t port, uint8_t pin);

/**
 * @brief 简单自旋延迟 (供测试用)
 * @param[in]  ms  毫秒数 (基于系统时钟的近似延迟)
 */
void gpio_delay_ms(uint32_t ms);
