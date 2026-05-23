/*
 * @brief 定时器驱动头文件 (S32K144)
 *        封装 SysTick 和 LPIT 定时器的寄存器操作
 *
 * @note 学习目标：理解定时器中断和精确延时
 *       对应书籍：第7章 定时器
 *
 * TODO：看书后实现以下函数
 *       1. systick_init()      — 初始化 SysTick 定时器
 *       2. systick_delay_ms()  — 基于 SysTick 的毫秒延时
 *       3. systick_get_tick()  — 获取系统心跳计数
 *       4. lpit_init()         — 初始化 LPIT 定时器
 *       5. lpit_start()        — 启动 LPIT 通道
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== SysTick 基地址 ========== */
/* SysTick 是 ARM Cortex-M4 内核外设，基地址固定 */
#define SYSTICK_BASE    (0xE000E010u)

/* ========== LPIT 基地址 ========== */
/* TODO §7.4：从 S32K144 参考手册中找到 LPIT 寄存器基地址 */
#define LPIT0_BASE      (0x40037000u)

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
 * TODO §7.3.2：实现 SysTick 初始化
 *       1. 计算重装载值: reload = cpu_freq_hz / 1000 - 1
 *       2. 写入 SYST_RVR 寄存器
 *       3. 写 SYST_CVR 寄存器清零
 *       4. 配置 SYST_CSR: ENABLE=1, TICKINT=1, CLKSOURCE=1
 */
int systick_init(uint32_t cpu_freq_hz);

/**
 * @brief 基于 SysTick 的毫秒级延时
 * @param[in]  ms  要延时的毫秒数
 *
 * TODO §7.3.2：实现精确延时
 *       1. 记录当前 tick 值
 *       2. 循环等待 tick 增加 ms
 */
void systick_delay_ms(uint32_t ms);

/**
 * @brief 获取当前系统心跳计数
 * @return  从上电以来经过的毫秒数
 *
 * TODO：在 SysTick_Handler 中递增的全局变量值
 */
uint64_t systick_get_tick(void);

/**
 * @brief 初始化 LPIT 通道
 *        配置单次/周期定时模式
 *
 * @param[in]  channel     LPIT 通道 (0~3)
 * @param[in]  period_us   定时周期 (微秒)
 * @param[in]  cpu_freq_hz CPU 主频
 * @return     0=成功, -1=参数错误
 *
 * TODO §7.4.2：实现 LPIT 初始化
 *       1. 使能 LPIT 时钟 (PCC_LPIT[CGC])
 *       2. 复位 LPIT (MCR[M_CEN] = 0)
 *       3. 配置通道定时值 TVAL[31:0]
 *       4. 配置通道控制 TCTRL:
 *          - T_EN: 使能
 *          - MODE: 0=周期性, 1=单次
 *          - TSOT: 0=软件触发, 1=外部触发
 *       5. 使能 LPIT (MCR[M_CEN] = 1)
 */
int lpit_init(lpit_channel_t channel, uint32_t period_us,
              uint32_t cpu_freq_hz);

/**
 * @brief 启动 LPIT 通道（软件触发）
 * @param[in]  channel   LPIT 通道
 *
 * TODO：设置 TCTRL[T_EN] = 1 启动定时器
 */
void lpit_start(lpit_channel_t channel);

/**
 * @brief 检查 LPIT 通道是否超时
 * @param[in]  channel   LPIT 通道
 * @return     true=超时, false=仍在计数
 *
 * TODO：检查 MSR[TIFn] 标志
 */
bool lpit_is_timeout(lpit_channel_t channel);

/**
 * @brief SysTick 中断处理函数
 *        在 SysTick_Handler 中被调用
 *
 * TODO：递增全局 tick 计数
 */
void SysTick_Handler(void);
