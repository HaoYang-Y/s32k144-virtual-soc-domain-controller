/**
 * @file    Mcu.c
 * @brief   AUTOSAR CP MCAL Mcu 驱动实现
 *
 * @note    内部调用 NXP SDK CLOCK_DRV API 完成时钟树初始化
 *          所有 SDK 依赖 (clock.h, CLOCK_DRV_*) 仅在本 .c 文件内可见
 */

#include "Mcu.h"
#include <stddef.h>             /* NULL */
#include "clock.h"              /* CLOCK_DRV_Init, CLOCK_DRV_GetFreq */
#include "status.h"             /* STATUS_SUCCESS */

/* ===================================================================
 *  Mcu_InitClock
 * =================================================================== */

void Mcu_InitClock(const Mcu_ConfigType *cfg)
{
    /*
     * 将 MCAL 配置映射到 SDK 调用:
     *   NULL / FIRC → 使用 SDK 默认配置 (FIRC 48MHz, div1/1/1)
     *   EXT_OSC      → 未来扩展: 切换 clock_manager_user_config_t 源
     */
    if ((cfg == NULL) || (cfg->source == MCU_CLOCK_SOURCE_FIRC)) {
        (void)CLOCK_DRV_Init(NULL);
    } else {
        /*
         * TODO: 外部晶振模式 — 需要构造 clock_manager_user_config_t
         *       设置 .scgConfig.soscConfig + .clockModeConfig 切换源
         */
        (void)CLOCK_DRV_Init(NULL);
    }
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