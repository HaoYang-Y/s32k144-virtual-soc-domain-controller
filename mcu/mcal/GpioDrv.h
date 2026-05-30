/*
 * @brief AUTOSAR MCAL GpioDrv 驱动头文件 (S32K144)
 *        封装 GPIO 引脚初始化和读写操作，使用 AUTOSAR 标准命名
 *
 * @note 对应 AUTOSAR CP MCAL 层 GPIO 驱动规范
 *       底层调用 NXP S32 SDK PINS_DRV API，
 *       上层 SWC 和 Com Stack 通过本接口间接访问 GPIO。
 */
#ifndef MCAL_GPIODRV_H
#define MCAL_GPIODRV_H

#include <stdint.h>
#include <stdbool.h>
#include "device_registers.h"
#include "pins_driver.h"

/* ========== AUTOSAR GPIO 类型定义 ========== */

/** @brief GPIO 通道 ID（高 8 位=端口号, 低 8 位=引脚号） */
typedef uint16_t Gpio_ChannelType;

/** @brief GPIO 引脚电平 */
typedef bool Gpio_PinLevelType;

/** @brief GPIO 通道方向 */
typedef enum {
    GPIO_CFG_DIR_INPUT  = 0,
    GPIO_CFG_DIR_OUTPUT = 1,
} GpioDrv_DirectionType;

/** @brief GPIO 通道上拉/下拉 */
typedef enum {
    GPIO_CFG_PULL_DISABLE = 0,
    GPIO_CFG_PULL_DOWN    = 1,
    GPIO_CFG_PULL_UP      = 2,
} GpioDrv_PullType;

/** @brief 单个 GPIO 通道配置 */
typedef struct {
    Gpio_ChannelType      channel;
    GpioDrv_DirectionType direction;
    GpioDrv_PullType      pull;
} GpioDrv_ChannelCfgType;

/** @brief GPIO 驱动配置容器 */
typedef struct {
    uint8_t                num_channels;
    const GpioDrv_ChannelCfgType *channels;
} GpioDrv_ConfigType;

/* ========== 通道 ID 宏 ========== */

/** @brief 从端口号和引脚号构造通道 ID
 *  @param port  端口号 (0=A, 1=B, 2=C, 3=D, 4=E)
 *  @param pin   引脚号 (0~31)
 */
#define GPIO_CH(port, pin)  ((Gpio_ChannelType)(((uint16_t)(port) << 8) | (uint16_t)(pin)))

/* ========== AUTOSAR GPIO 驱动接口 ========== */

/**
 * @brief 初始化 GPIO 驱动及所有通道
 *
 * @param[in]  cfg   GPIO 通道配置数组指针
 *
 * @note 批量初始化 cfg->channels 中所有通道的 PORT PCR 和 PDDR
 */
void GpioDrv_Init(const GpioDrv_ConfigType *cfg);

/**
 * @brief 读取指定 GPIO 通道的输入电平
 *
 * @param[in]  channel   GPIO 通道 ID
 * @param[out] level     读取到的电平值
 * @return     0=成功, -1=通道无效
 *
 * @note 对应 AUTOSAR API: Gpio_ReadPin(channel, &level)
 */
int Gpio_ReadPin(Gpio_ChannelType channel, Gpio_PinLevelType *level);

/**
 * @brief 设置指定 GPIO 通道的输出电平
 *
 * @param[in]  channel   GPIO 通道 ID
 * @param[in]  level     要输出的电平值
 * @return     0=成功, -1=通道无效
 *
 * @note 对应 AUTOSAR API: Gpio_WritePin(channel, level)
 */
int Gpio_WritePin(Gpio_ChannelType channel, Gpio_PinLevelType level);

#endif /* MCAL_GPIODRV_H */
