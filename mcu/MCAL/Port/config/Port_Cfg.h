/**
 * @file    Port_Cfg.h
 * @brief   Port 模块预编译配置 — 引脚复用映射
 *
 * @note    当前通过 NXP SDK pin_mux.h 配置，此文件为 AUTOSAR 配置规范参考
 */

#ifndef PORT_CFG_H
#define PORT_CFG_H

#include "Std_Types.h"

/** @brief 引脚功能选择 */
typedef enum {
    PORT_FUNC_GPIO    = 0,
    PORT_FUNC_ALT1    = 1,
    PORT_FUNC_ALT2    = 2,
    PORT_FUNC_ALT3    = 3,
    PORT_FUNC_ALT4    = 4,
    PORT_FUNC_ALT5    = 5,
    PORT_FUNC_ALT6    = 6,
    PORT_FUNC_ALT7    = 7,
} Port_PinModeType;

/** @brief 已使用引脚数 */
#define PORT_CONFIGURED_PINS_COUNT  4U

#endif /* PORT_CFG_H */