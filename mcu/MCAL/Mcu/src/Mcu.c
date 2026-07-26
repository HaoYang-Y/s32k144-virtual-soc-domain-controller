/**
 * @file    Mcu.c
 * @brief   AUTOSAR CP MCAL Mcu 驱动实现
 */

#include "Mcu.h"
#include <stddef.h>
#include "clock.h"
#include "status.h"
#include "device_registers.h"   /* RCM, S32_SCB */

/* ===================================================================
 *  Mcu_Init (SWS_Mcu_00013)
 * =================================================================== */

Std_ReturnType Mcu_Init(const Mcu_ConfigType *cfg)
{
    if ((cfg == NULL) || (cfg->source == MCU_CLOCK_SOURCE_FIRC)) {
        (void)CLOCK_DRV_Init(NULL);
    } else {
        /* TODO: 外部晶振模式 */
        (void)CLOCK_DRV_Init(NULL);
    }
    return E_OK;
}

/* ===================================================================
 *  Mcu_GetCoreFreq
 * =================================================================== */

uint32_t Mcu_GetCoreFreq(void)
{
    uint32_t freq = 48000000U;
    (void)CLOCK_DRV_GetFreq(CORE_CLK, &freq);
    return freq;
}

/* ===================================================================
 *  Mcu_GetResetReason (SWS_Mcu_00015)
 * =================================================================== */

Mcu_ResetType Mcu_GetResetReason(void)
{
    uint32_t srs = RCM->SRS;

    if (srs & RCM_SRS_POR_MASK)    return MCU_RESET_POWER_ON;
    if (srs & RCM_SRS_WDOG_MASK)   return MCU_RESET_WATCHDOG;
    if (srs & RCM_SRS_LOCKUP_MASK)  return MCU_RESET_SOFTWARE; /* 硬件 fault */
    if (srs & RCM_SRS_PIN_MASK)    return MCU_RESET_EXTERNAL;
    return MCU_RESET_UNKNOWN;
}

/* ===================================================================
 *  Mcu_PerformReset (SWS_Mcu_00093)
 * =================================================================== */

void Mcu_PerformReset(void)
{
    /* 使用 SDK 的 S32_SCB 结构 (CMSIS SCB 被 SDK 重命名为 S32_SCB) */
    __disable_irq();
    S32_SCB->AIRCR = ((0x5FAUL << 16U) | S32_SCB_AIRCR_SYSRESETREQ_MASK);
    __DSB();
    for (;;) { __WFI(); }
}
