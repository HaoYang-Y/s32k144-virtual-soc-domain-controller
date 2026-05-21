/*
 * @brief GPIO 驱动头文件 (S32K144)
 *        封装 GPIO 模块的寄存器操作，提供 LED 控制、按键读取等基本功能
 *
 * @note 学习目标：理解 GPIO 寄存器的直接操作方式
 *       对应书籍：第4章 GPIO及程序框架 (§4.2~§4.3)
 *
 * TODO：看书后实现以下函数
 *       1. gpio_init()    — 配置引脚方向、上拉/下拉
 *       2. gpio_write()   — 输出高/低电平
 *       3. gpio_read()    — 读取输入电平
 *       4. gpio_toggle()  — 翻转输出电平
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== GPIO 端口基地址 ========== */
/* TODO §4.2：从 S32K144 参考手册中找到 GPIO 寄存器基地址 */
#define GPIO_PTA_BASE  (0x400FF000u)
#define GPIO_PTB_BASE  (0x400FF040u)
#define GPIO_PTC_BASE  (0x400FF080u)
#define GPIO_PTD_BASE  (0x400FF0C0u)

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
 *        1. 使能 PORT 时钟 (通过 PCC 寄存器)
 *        2. 配置 PORT PCR 寄存器 (MUX 复用功能 + 上拉/下拉)
 *        3. 配置 GPIO PDDR 寄存器 (方向)
 *
 * @param[in]  port   端口 (A/B/C/D)
 * @param[in]  pin    引脚号 (0~31)
 * @param[in]  dir    方向 (输入/输出)
 * @param[in]  pull   上拉/下拉配置
 * @return     0=成功, -1=参数错误
 *
 * TODO：阅读 §4.2.2 GPIO 模块内部寄存器，实现此函数
 */
int gpio_init(gpio_port_t port, uint8_t pin,
              gpio_direction_t dir, gpio_pull_t pull);

/**
 * @brief 设置 GPIO 引脚输出电平
 * @param[in]  port   端口
 * @param[in]  pin    引脚号
 * @param[in]  value  true=高电平, false=低电平
 *
 * TODO：使用 PSOR/PCOR 寄存器实现（§4.2.2）
 *       提示：高电平用 PSOR（置位），低电平用 PCOR（清零）
 */
void gpio_write(gpio_port_t port, uint8_t pin, bool value);

/**
 * @brief 读取 GPIO 引脚输入电平
 * @param[in]  port   端口
 * @param[in]  pin    引脚号
 * @return     true=高电平, false=低电平
 *
 * TODO：使用 PDIR 寄存器实现
 */
bool gpio_read(gpio_port_t port, uint8_t pin);

/**
 * @brief 翻转 GPIO 引脚输出电平
 * @param[in]  port   端口
 * @param[in]  pin    引脚号
 *
 * TODO：使用 PTOR 寄存器实现
 */
void gpio_toggle(gpio_port_t port, uint8_t pin);

/**
 * @brief 简单自旋延迟 (供测试用)
 * @param[in]  ms  毫秒数 (基于 48MHz 的近似延迟)
 *
 * TODO：实现一个简单的 for 循环延迟
 *       提示：48MHz 下 1ms ≈ 16000 条空循环
 */
void gpio_delay_ms(uint32_t ms);
