/**
 * @file    Mcu.h
 * @brief   AUTOSAR CP MCAL Mcu 驱动 — MCU 时钟系统初始化
 *
 * @note    底层调用 NXP S32 SDK CLOCK_DRV API，头文件不暴露任何 SDK 类型
 */

#ifndef MCAL_MCU_H
#define MCAL_MCU_H

#include <stdint.h>

/* ===================================================================
 *  类型定义 (AUTOSAR MCAL 自有，不含 SDK 依赖)
 * =================================================================== */

/** @brief MCU 系统时钟源 */
typedef enum {
    MCU_CLOCK_SOURCE_FIRC    = 0,  /**< 48MHz 内部快速 RC */
    MCU_CLOCK_SOURCE_EXT_OSC = 1,  /**< 外部晶振 */
} Mcu_ClockSourceType;

/** @brief MCU 时钟配置 — 仅含 MCAL 自有类型 */
typedef struct {
    Mcu_ClockSourceType source;
    uint32_t            core_freq_hz;
    uint32_t            bus_freq_hz;
} Mcu_ConfigType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 初始化 MCU 时钟系统
 * @param cfg  时钟配置 (NULL = 使用默认 FIRC 48MHz)
 */
void     Mcu_InitClock(const Mcu_ConfigType *cfg);

/**
 * @brief 获取当前核心时钟频率 (Hz)
 */
uint32_t Mcu_GetCoreFreq(void);

#endif /* MCAL_MCU_H */