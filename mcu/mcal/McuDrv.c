/*
 * @brief AUTOSAR MCAL McuDrv 驱动实现 (S32K144)
 *        通过 NXP SDK CLOCK_DRV API 实现 MCU 时钟系统初始化
 *
 * @note 使用 S32K1xx SDK platform/drivers 层的标准接口：
 *       - CLOCK_DRV_Init()      初始化时钟树
 *       - CLOCK_DRV_GetFreq()   获取指定时钟频率
 *
 *       原版本（commit f7a8573）通过直接操作 SCG CSR 寄存器实现：
 *       SCG CSR[31:28] 读取时钟源
 *       现统一委托给 SDK 驱动层，提升可移植性。
 */
#include "McuDrv.h"
#include "clock.h"
#include <stddef.h>

/**
 * @brief 初始化 MCU 时钟系统
 *
 * @param[in]  cfg   时钟配置（NULL=使用 SDK 默认配置 FIRC 48MHz）
 *
 * @note 调用 SDK CLOCK_DRV_Init()，传入 NULL 时使用 SDK 默认时钟配置：
 *       系统时钟 FIRC 48MHz，Core/Bus/Flash 均为 48MHz。
 *       后续可传入 clock_user_config_t 结构体配置外部晶振或 PLL。
 */
void Mcu_InitClock(const McuDrv_ConfigType *cfg)
{
    /*
     * 使用 SDK CLOCK_DRV_Init() 初始化完整时钟树
     * NULL = 使用 SDK 内置默认配置 (FIRC 48MHz, div1/1/1)
     */
    (void)CLOCK_DRV_Init(NULL);

    (void)cfg;
}

/**
 * @brief 获取当前核心时钟频率
 *
 * @return 核心时钟频率 (Hz)
 *
 * @note 调用 SDK CLOCK_DRV_GetFreq() 查询 CORE_CLOCK 真实频率。
 *       若 SDK 未初始化（返回错误），回退返回默认 48MHz。
 */
uint32_t Mcu_GetCoreFreq(void)
{
    uint32_t freq = 0U;
    status_t status;

    status = CLOCK_DRV_GetFreq(CORE_CLK, &freq);
    if (status != STATUS_SUCCESS)
    {
        freq = 48000000U;
    }

    return freq;
}
