/**
 * @file    Gpio.h
 * @brief   AUTOSAR CP MCAL Gpio 驱动 — GPIO 读写封装
 *
 * @note    对应 AUTOSAR CP Dio + Port 简化合并
 *          底层调用 NXP S32 SDK PINS_DRV API，头文件不暴露任何 SDK 类型
 */

#ifndef MCAL_GPIO_H
#define MCAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* ===================================================================
 *  类型定义 (AUTOSAR MCAL 自有，不含 SDK 依赖)
 * =================================================================== */

/** @brief GPIO 通道 ID (高 8 位 = 端口号 A~E, 低 8 位 = 引脚号 0~31) */
typedef uint16_t Gpio_ChannelType;

/** @brief GPIO 引脚电平 */
typedef bool Gpio_PinLevelType;

/** @brief GPIO 方向 */
typedef enum {
    GPIO_DIR_INPUT  = 0,
    GPIO_DIR_OUTPUT = 1,
} Gpio_DirectionType;

/** @brief GPIO 上拉/下拉 */
typedef enum {
    GPIO_PULL_DISABLE = 0,
    GPIO_PULL_DOWN    = 1,
    GPIO_PULL_UP      = 2,
} Gpio_PullType;

/** @brief 单个 GPIO 通道配置 */
typedef struct {
    Gpio_ChannelType   channel;
    Gpio_DirectionType direction;
    Gpio_PullType       pull;
} Gpio_ChannelConfigType;

/** @brief GPIO 驱动配置容器 */
typedef struct {
    uint8_t                    num_channels;
    const Gpio_ChannelConfigType *channels;
} Gpio_ConfigType;

/* ===================================================================
 *  通道 ID 构造宏
 * =================================================================== */

/** @brief 从端口号和引脚号构造通道 ID
 *  @param port  端口号 (0=A, 1=B, 2=C, 3=D, 4=E)
 *  @param pin   引脚号 (0~31)
 */
#define GPIO_CH(port, pin) \
    ((Gpio_ChannelType)(((uint16_t)(port) << 8) | (uint16_t)(pin)))

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 初始化 GPIO 驱动 — 批量配置所有通道
 * @param cfg  GPIO 通道配置 (NULL 则跳过)
 */
void Gpio_Init(const Gpio_ConfigType *cfg);

/**
 * @brief 读取指定通道的输入电平
 * @param channel  GPIO 通道 ID
 * @param level    输出: 电平值
 * @return          0=成功, -1=通道无效
 */
int  Gpio_ReadPin(Gpio_ChannelType channel, Gpio_PinLevelType *level);

/**
 * @brief 设置指定通道的输出电平
 * @param channel  GPIO 通道 ID
 * @param level    输出电平值
 * @return          0=成功, -1=通道无效
 */
int  Gpio_WritePin(Gpio_ChannelType channel, Gpio_PinLevelType level);

#endif /* MCAL_GPIO_H */