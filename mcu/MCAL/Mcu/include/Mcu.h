/**
 * @file    Mcu.h
 * @brief   AUTOSAR CP MCAL Mcu 驱动 — MCU 时钟与复位管理
 *
 * @note    对标 AUTOSAR SWS_Mcu:
 *          - SWS_Mcu_00013: Mcu_Init
 *          - SWS_Mcu_00046: Mcu_SetMode (TODO)
 *          - SWS_Mcu_00015: Mcu_GetResetReason
 *          - SWS_Mcu_00093: Mcu_PerformReset
 *          底层调用 NXP S32 SDK CLOCK_DRV API
 */

#ifndef MCAL_MCU_H
#define MCAL_MCU_H

#include <stdint.h>
#include "Std_Types.h"

/* ===================================================================
 *  类型定义
 * =================================================================== */

typedef enum {
    MCU_CLOCK_SOURCE_FIRC    = 0,
    MCU_CLOCK_SOURCE_EXT_OSC = 1,
} Mcu_ClockSourceType;

typedef struct {
    Mcu_ClockSourceType source;
    uint32_t            core_freq_hz;
    uint32_t            bus_freq_hz;
} Mcu_ConfigType;

/** @brief 复位原因 (SWS_Mcu_00015) */
typedef enum {
    MCU_RESET_POWER_ON    = 0U,
    MCU_RESET_WATCHDOG    = 1U,
    MCU_RESET_SOFTWARE    = 2U,
    MCU_RESET_EXTERNAL    = 3U,
    MCU_RESET_UNKNOWN     = 0xFFU,
} Mcu_ResetType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/** @brief 初始化 MCU 时钟与电源 (SWS_Mcu_00013) */
Std_ReturnType Mcu_Init(const Mcu_ConfigType *cfg);

/** @brief 获取当前核心时钟频率 (Hz) */
uint32_t Mcu_GetCoreFreq(void);

/** @brief 获取上次复位原因 (SWS_Mcu_00015) */
Mcu_ResetType Mcu_GetResetReason(void);

/** @brief 执行软件复位 (SWS_Mcu_00093) */
void Mcu_PerformReset(void);

#endif /* MCAL_MCU_H */
