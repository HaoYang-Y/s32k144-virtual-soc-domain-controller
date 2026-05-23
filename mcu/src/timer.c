/*
 * @brief 定时器驱动实现 (S32K144)
 *        基于 NXP S32 SDK LPIT_DRV API 和 ARM CMSIS SysTick 实现定时功能
 *
 * @note SysTick: ARM Cortex-M4 内核定时器，用于提供 1ms 系统心跳
 *       LPIT: 利用 NXP S32 SDK LPIT_DRV 驱动完成通道定时
 *
 *       SDK API 涉及:
 *         - LPIT_DRV_Init():       初始化 LPIT 模块
 *         - LPIT_DRV_StartChannels(): 启动指定通道
 *         - LPIT_DRV_GetChannelsFlag(): 检查超时标志
 */
#include "timer.h"

/* NXP S32 SDK LPIT 驱动头文件 */
#include "lpit_driver.h"

/* ARM CMSIS 核心头文件 (提供 SysTick 寄存器定义) */
#include "core_cm4.h"

/* ========== 全局变量 ========== */

/** @brief 系统心跳计数器，在 SysTick_Handler 中每次中断递增 1 */
static volatile uint64_t system_tick = 0u;

/* ========== 函数实现 ========== */

int systick_init(uint32_t cpu_freq_hz)
{
    uint32_t reload_val;

    /* 参数检查 */
    if (cpu_freq_hz == 0u)
    {
        return -1;
    }

    /*
     * 计算 SysTick 重装载值
     * SysTick 时钟 = CPU 时钟 (CLKSOURCE=1), 每 1ms 触发一次中断
     * reload = cpu_freq_hz / 1000 - 1
     */
    reload_val = cpu_freq_hz / 1000u - 1u;

    /*
     * ARM CMSIS API: SysTick_Config(reload_val)
     * 该函数内部自动完成:
     *   - 设置 SYST_RVR = reload_val
     *   - 清零 SYST_CVR
     *   - 设置 SYST_CSR = ENABLE | TICKINT | CLKSOURCE
     */
    if (SysTick_Config(reload_val) != 0)
    {
        return -1;
    }

    return 0;
}

void systick_delay_ms(uint32_t ms)
{
    uint64_t start_tick;

    /* 记录起始时间戳 */
    start_tick = system_tick;

    /* 轮询等待直到经过指定毫秒数 */
    while ((system_tick - start_tick) < (uint64_t)ms)
    {
        /* 空循环等待 */
    }
}

uint64_t systick_get_tick(void)
{
    /*
     * 返回系统心跳计数
     * 在 SysTick_Handler 中每次中断递增一次 (1ms)
     */
    return system_tick;
}

int lpit_init(lpit_channel_t channel, uint32_t period_us,
              uint32_t cpu_freq_hz)
{
    lpit_user_config_t lpitConfig;
    lpit_user_channel_config_t channelConfig;

    /* 参数检查 */
    if ((channel > LPIT_CHANNEL_3) || (period_us == 0u) ||
        (cpu_freq_hz == 0u))
    {
        return -1;
    }

    /*
     * 配置 LPIT 模块参数 (lpit_user_config_t)
     * SDK 参考: lpit_driver.h 中的 lpit_user_config_t 结构体
     */
    lpitConfig.enableInDebug = false;

    /* 调用 SDK API 初始化 LPIT 模块 */
    LPIT_DRV_Init(&lpitConfig);

    /*
     * 配置指定通道参数 (lpit_user_channel_config_t)
     * - 定时模式: 周期性 (kLpitTimerModePeriodic)
     * - 触发源:   软件触发 (kLpitTriggerSourceSoftware)
     * - 定时周期: period_us (微秒)
     * - 时钟源:   按照 cpu_freq_hz 计算 TVAL
     */
    channelConfig.timerMode       = LPIT_PERIODIC_COUNTER;
    channelConfig.triggerSource   = LPIT_TRIGGER_SOURCE_INTERNAL;
    channelConfig.triggerSelect   = 0u;
    channelConfig.periodUs        = period_us;
    channelConfig.channelEnable   = false;  /* 暂不使能，由 lpit_start() 启动 */

    /* 调用 SDK API 配置通道 */
    LPIT_DRV_InitChannel(channel, &channelConfig);

    return 0;
}

void lpit_start(lpit_channel_t channel)
{
    /*
     * 调用 SDK API 启动指定 LPIT 通道
     * SDK 底层:
     *   1. 向 SET_T_EN_n 寄存器对应位写 1
     *   2. 通道开始计数
     */
    LPIT_DRV_StartChannels(LPTIMER_GetChannelMask(channel));
}

bool lpit_is_timeout(lpit_channel_t channel)
{
    uint32_t flags;

    /* 调用 SDK API 获取超时标志 */
    flags = LPIT_DRV_GetChannelsFlag(LPTIMER_GetChannelMask(channel));

    /* 检查对应通道的超时标志 */
    return (bool)((flags & (1u << (uint32_t)channel)) != 0u);
}

/*
 * @brief SysTick 中断处理函数
 *        每次 SysTick 中断递增系统心跳计数器
 *
 * @note 该函数由中断向量表自动调用
 *       无需手动清除中断标志位 (硬件自动完成)
 */
void SysTick_Handler(void)
{
    system_tick++;
}
