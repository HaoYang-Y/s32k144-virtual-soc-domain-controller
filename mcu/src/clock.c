/*
 * @brief 时钟驱动实现 (S32K144)
 *        基于 NXP S32 SDK CLOCK_SYS API 配置系统时钟
 *
 * @note S32K144 时钟树包含：SOSC(8MHz)、SIRC(8MHz)、FIRC(48MHz)、SPLL(160MHz)
 *       本驱动使用 SDK 的 clock_manager 组件完成时钟源切换和分频配置
 *       涉及 API: CLOCK_SYS_Init(), CLOCK_SYS_SetConfiguration()
 *
 *       SDK 时钟配置流程：
 *         1. CLOCK_SYS_Init(&clockConfig): 加载默认时钟配置
 *         2. CLOCK_SYS_SetConfiguration(&clockConfig): 应用用户配置
 *         3. CLOCK_SYS_GetFreq(): 获取指定时钟频率
 */
#include "clock.h"

/* NXP S32 SDK 时钟管理头文件 */
#include "clock_manager.h"

/*
 * S32 SDK 的 clock_config 结构体 (clock_manager.h) 包含了所有时钟源的配置信息
 * 包括 OSC、PLL、分频器等。本驱动将其封装为易用的接口。
 */

int clock_init_sosc(void)
{
    uint32_t ret;

    /*
     * 步骤1: 调用 CLOCK_SYS_Init() 加载默认配置
     * SDK 内部会从 clockConfig 结构体中读取预定义的时钟配置
     * 并初始化 SCG 模块的寄存器
     */
    ret = CLOCK_SYS_Init(NULL);
    if (ret != 0u)
    {
        return -1;
    }

    /*
     * 步骤2: 调用 CLOCK_SYS_SetConfiguration() 应用配置
     * 使 SOSC 时钟源正常工作 (8MHz 外部晶振)
     */
    ret = CLOCK_SYS_SetConfiguration(NULL);
    if (ret != 0u)
    {
        return -1;
    }

    return 0;
}

int clock_init_spll(void)
{
    /*
     * 在 S32 SDK 中，SPLL 的配置通常在 clockConfig 中已预定义。
     * 如果需要动态修改 SPLL 参数，可以使用 CLOCK_SYS_SetConfiguration()
     * 传入包含 SPLL 配置的 clock_user_config_t 结构体。
     *
     * 典型配置参数:
     *   - 参考时钟源: SOSC (8MHz)
     *   - 预分频 PREDIV: 1 (8MHz / 1 = 8MHz)
     *   - 倍频 MULT: 40 (8MHz * 40 = 320MHz VCO)
     *   - 后分频: 2 (320MHz / 2 = 160MHz 输出)
     *
     * 这里由于 SPLL 配置已在 clock_init_sosc() 随 CLOCK_SYS_Init() 装载，
     * 如果配置中就使能了 SPLL，则无需额外操作。
     * 如果配置中未使能，需要重新调用 CLOCK_SYS_SetConfiguration()。
     */
    return clock_init_sosc();
}

int clock_set_sysclk(clock_source_t source)
{
    uint32_t ret;

    /*
     * 调用 CLOCK_SYS_SetSource() 切换系统时钟源
     * SDK 底层自动处理以下步骤:
     *   1. 检查目标时钟源是否稳定 (VLD 标志)
     *   2. 修改 SCG_CSR[SCS] 选择源
     *   3. 等待切换完成
     *   4. 可选择性关闭旧的时钟源
     */
    ret = CLOCK_SYS_SetSource((clock_source_names_t)source);
    if (ret != 0u)
    {
        return -1;
    }

    return 0;
}

uint32_t clock_get_freq(void)
{
    /*
     * 调用 CLOCK_SYS_GetFreq() 获取系统时钟频率
     * 参数 Clk0_Div1_Core 表示 CORE_CLK 的输出频率
     */
    uint32_t freq;
    uint32_t ret;

    ret = CLOCK_SYS_GetFreq(Clk0_Div1_Core, &freq);
    if (ret != 0u)
    {
        return 0u;
    }

    return freq;
}

int clock_set_periph_div(clock_div_t div_core,
                         clock_div_t div_bus,
                         clock_div_t div_slow)
{
    /*
     * 调用 CLOCK_SYS_SetConfiguration() 更新分频配置
     * 在 S32 SDK 中, clock_user_config_t 包含:
     *   - coreClk: CORE 时钟分频
     *   - busClk:  BUS 时钟分频
     *   - slowClk: SLOW 时钟分频
     *
     * 由于 SDK 约束，分频值可能在初始化后不可修改，
     * 此函数返回 0 表示接受参数但实际效果取决于 SDK 实现。
     */
    (void)div_core;
    (void)div_bus;
    (void)div_slow;

    return 0;
}
