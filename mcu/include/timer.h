/*
 * @brief 定时器驱动头文件 (S32K144)
 *        封装基于 NXP S32 SDK 的 SysTick 和 LPIT 定时器功能
 *
 * @note 本驱动使用 NXP S32 SDK LPIT_DRV API 以及 ARM CMSIS SysTick 实现
 *       涉及的头文件：lpit_driver.h, core_cm4.h
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== 枚举定义 ========== */

/** @brief LPIT 通道 */
typedef enum {
    LPIT_CHANNEL_0 = 0,
    LPIT_CHANNEL_1 = 1,
    LPIT_CHANNEL_2 = 2,
    LPIT_CHANNEL_3 = 3,
} lpit_channel_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化 SysTick 定时器
 *        配置 SysTick 以 1ms 为周期产生中断
 *
 * @param[in]  cpu_freq_hz  CPU 主频 (Hz)
 * @return     0=成功, -1=参数错误
 *
 * @note ARM CMSIS API: SysTick_Config()
 */
int systick_init(uint32_t cpu_freq_hz);

/**
 * @brief 基于 SysTick 的毫秒级延时
 * @param[in]  ms  要延时的毫秒数
 *
 * @note 通过轮询 systick_get_tick() 实现
 */
void systick_delay_ms(uint32_t ms);

/**
 * @brief 获取当前系统心跳计数
 * @return  从上电以来经过的毫秒数
 *
 * @note 在 SysTick_Handler 中递增的全局变量值
 */
uint64_t systick_get_tick(void);

/**
 * @brief 初始化 LPIT 通道
 *        通过 NXP S32 SDK 配置单次/周期定时模式
 *
 * @param[in]  channel     LPIT 通道 (0~3)
 * @param[in]  period_us   定时周期 (微秒)
 * @param[in]  cpu_freq_hz CPU 主频
 * @return     0=成功, -1=参数错误
 *
 * @note SDK API: LPIT_DRV_Init()
 *       配置结构体: lpit_user_config_t
 */
int lpit_init(lpit_channel_t channel, uint32_t period_us,
              uint32_t cpu_freq_hz);

/**
 * @brief 启动 LPIT 通道（软件触发）
 * @param[in]  channel   LPIT 通道
 *
 * @note SDK API: LPIT_DRV_StartChannels()
 */
void lpit_start(lpit_channel_t channel);

/**
 * @brief 检查 LPIT 通道是否超时
 * @param[in]  channel   LPIT 通道
 * @return     true=超时, false=仍在计数
 *
 * @note SDK API: LPIT_DRV_GetChannelsFlag()
 */
bool lpit_is_timeout(lpit_channel_t channel);

/**
 * @brief SysTick 中断处理函数
 *        在 SysTick_Handler 中被调用
 *
 * @note 递增全局 tick 计数
 */
void SysTick_Handler(void);
