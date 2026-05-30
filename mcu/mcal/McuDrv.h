/*
 * @brief AUTOSAR MCAL McuDrv 驱动头文件 (S32K144)
 *        封装 MCU 时钟系统初始化，使用 AUTOSAR 标准命名
 *
 * @note 对应 AUTOSAR CP MCAL 层 Mcu 驱动规范
 *       底层调用 NXP S32 SDK CLOCK_DRV API，
 *       负责 SOC 启动时钟和系统基础时钟配置
 */
#ifndef MCAL_MCUDRV_H
#define MCAL_MCUDRV_H

#include <stdint.h>

/* ========== 时钟源选择 ========== */

/** @brief MCU 系统时钟源 */
typedef enum {
    MCU_CLK_FIRC      = 0,  /**< 48MHz 内部快速 RC */
    MCU_CLK_EXT_OSC   = 1,  /**< 外部晶振 (预留) */
} McuDrv_ClockSourceType;

/** @brief MCU 时钟配置 */
typedef struct {
    McuDrv_ClockSourceType source;  /**< 系统时钟源 */
    uint32_t                core_freq_hz;  /**< 期望核心频率 */
    uint32_t                bus_freq_hz;   /**< 期望总线频率 */
} McuDrv_ConfigType;

/* ========== AUTOSAR MCU 驱动接口 ========== */

/**
 * @brief 初始化 MCU 时钟系统
 *
 * @param[in]  cfg   时钟配置（NULL=使用 SDK 默认配置 FIRC 48MHz）
 *
 * @note 委托给 SDK CLOCK_DRV_Init() 完成时钟树初始化，
 *       默认配置: FIRC 48MHz 作为系统时钟，Core/Bus/Flash 均运行
 *       在 48MHz。可扩展支持外部晶振、PLL 倍频。
 */
void Mcu_InitClock(const McuDrv_ConfigType *cfg);

/**
 * @brief 获取当前核心时钟频率
 *
 * @return 核心时钟频率 (Hz)
 */
uint32_t Mcu_GetCoreFreq(void);

#endif /* MCAL_MCUDRV_H */
