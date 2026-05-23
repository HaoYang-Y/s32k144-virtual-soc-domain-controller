/*
 * @brief 时钟驱动头文件 (S32K144)
 *        封装基于 NXP S32 SDK 的时钟配置功能
 *
 * @note 本驱动使用 NXP S32 SDK CLOCK_SYS API 实现
 *       涉及的头文件：clock.h (SDK 自带)
 *
 *       S32K144 时钟树：
 *       ┌──────┐    ┌──────┐    ┌────────┐
 *       │SOSC  │    │SIRC  │    │SPLL    │
 *       │8MHz  │    │8MHz  │    │160MHz  │
 *       └──┬───┘    └──┬───┘    └───┬────┘
 *          │            │            │
 *          └──────┬─────┘            │
 *                 │ SCG              │
 *           ┌─────▼──────┐   ┌──────▼──────┐
 *           │ SYS_CLK    │   │ CORE_CLK     │
 *           │ (系统时钟) │   │ (内核时钟)   │
 *           └────────────┘   └──────────────┘
 */
#pragma once

#include <stdint.h>

/* ========== 时钟源频率定义 ========== */
/* S32K144 EVB 评估板配置 */
#define SOSC_CLK_FREQ       (8000000u)   /* 外部 8MHz 晶振 */
#define SIRC_CLK_FREQ       (8000000u)   /* 内部 8MHz RC 振荡器 */
#define FIRC_CLK_FREQ       (48000000u)  /* 内部 48MHz 快速 RC 振荡器 */
#define SPLL_DESIRED_FREQ   (160000000u) /* SPLL 目标 160MHz */

/* ========== 枚举定义 ========== */

/** @brief 系统时钟源选择 */
typedef enum {
    CLOCK_SRC_SOSC  = 0x01,  /**< 外部晶振 (8MHz) */
    CLOCK_SRC_SIRC  = 0x02,  /**< 内部慢速 RC (8MHz) */
    CLOCK_SRC_FIRC  = 0x03,  /**< 内部快速 RC (48MHz) */
    CLOCK_SRC_SPLL  = 0x06,  /**< SPLL 锁相环 (160MHz) */
} clock_source_t;

/** @brief 时钟分频值 */
typedef enum {
    CLOCK_DIV_1   = 0,
    CLOCK_DIV_2   = 1,
    CLOCK_DIV_3   = 2,
    CLOCK_DIV_4   = 3,
    CLOCK_DIV_5   = 4,
    CLOCK_DIV_6   = 5,
    CLOCK_DIV_7   = 6,
    CLOCK_DIV_8   = 7,
} clock_div_t;

/** @brief PLL 锁相环的时钟源选择 */
typedef enum {
    SPLL_SRC_SOSC = 0,   /**< SPLL 参考时钟=外部晶振 */
    SPLL_SRC_SIRC = 1,   /**< SPLL 参考时钟=内部 SIRC */
    SPLL_SRC_FIRC = 2,   /**< SPLL 参考时钟=内部 FIRC */
} spll_source_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化外部晶振 (SOSC)
 *        通过 NXP S32 SDK 配置 SOSC 为 8MHz 外部晶振模式
 *
 * @return 0=成功, -1=超时
 *
 * @note SDK API: CLOCK_SYS_Init() + CLOCK_SYS_SetConfiguration()
 */
int clock_init_sosc(void);

/**
 * @brief 初始化 SPLL (系统 PLL)
 *        使用外部晶振 (SOSC) 作为参考时钟，目标输出 160MHz
 *
 * @return 0=成功, -1=超时
 *
 * @note SDK API: CLOCK_SYS_SetConfiguration()
 *       配置结构体: clock_user_config_t
 */
int clock_init_spll(void);

/**
 * @brief 选择系统时钟源
 *
 * @param[in]  source   系统时钟源选择
 * @return     0=成功, -1=参数无效或切换失败
 *
 * @note SDK API: CLOCK_SYS_SetSource()
 */
int clock_set_sysclk(clock_source_t source);

/**
 * @brief 获取当前系统时钟频率
 *
 * @return 当前系统时钟频率 (Hz)
 *
 * @note SDK API: CLOCK_SYS_GetFreq()
 */
uint32_t clock_get_freq(void);

/**
 * @brief 配置外设时钟分频
 *
 * @param[in]  div_core   CORE 时钟分频
 * @param[in]  div_bus    BUS 时钟分频
 * @param[in]  div_slow   SLOW 时钟分频
 * @return     0=成功, -1=参数错误
 *
 * @note SDK API: CLOCK_SYS_SetConfiguration()
 */
int clock_set_periph_div(clock_div_t div_core,
                         clock_div_t div_bus,
                         clock_div_t div_slow);
