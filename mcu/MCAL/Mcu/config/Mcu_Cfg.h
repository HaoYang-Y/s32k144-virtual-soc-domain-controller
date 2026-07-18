/**
 * @file    Mcu_Cfg.h
 * @brief   MCU 驱动预编译配置 — 时钟源和频率
 *
 * @note    对应 AUTOSAR CP MCU 模块编译期配置
 *          当前: FIRC 48MHz 主时钟, SOSC 8MHz 外部振荡器 (FlexCAN0 时钟源)
 */

#ifndef MCU_CFG_H
#define MCU_CFG_H

#include "Std_Types.h"

#define MCU_CORE_CLK_FREQ_HZ     48000000UL   /**< 内核时钟 (FIRC 48MHz) */
#define MCU_BUS_CLK_FREQ_HZ      48000000UL   /**< 总线时钟 */
#define MCU_FLEXCAN0_CLK_FREQ_HZ 8000000UL    /**< FlexCAN0 PE 时钟 (SOSC 8MHz, CAN 真实通信) */
#define MCU_SLOW_CLK_FREQ_HZ     4000000UL    /**< 慢速时钟 */
#define MCU_FIRCDIV2_CLK_FREQ_HZ 48000000UL   /**< FIRCDIV2 = 48MHz, SPI/UART 外设时钟源 */

#endif /* MCU_CFG_H */