/*
 * @brief 时钟驱动头文件 (S32K144)
 *        封装时钟系统的寄存器操作，提供时钟配置功能
 *
 * @note 学习目标：理解 S32K144 时钟树和 PLL 配置
 *       对应书籍：第5章 时钟系统
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
 *
 * TODO：看书后实现以下函数
 *       1. clock_init_sosc()  — 配置外部晶振
 *       2. clock_init_spll()  — 配置 PLL 锁相环
 *       3. clock_set_sysclk() — 选择系统时钟源
 *       4. clock_get_freq()   — 获取当前时钟频率
 */
#pragma once

#include <stdint.h>

/* ========== 时钟源频率定义 ========== */
/* S32K144 EVB 评估板配置 */
#define SOSC_CLK_FREQ       (8000000u)   /* 外部 8MHz 晶振 */
#define SIRC_CLK_FREQ       (8000000u)   /* 内部 8MHz RC 振荡器 */
#define FIRC_CLK_FREQ       (48000000u)  /* 内部 48MHz 快速 RC 振荡器 */
#define SPLL_DESIRED_FREQ   (160000000u) /* SPLL 目标 160MHz */

/* ========== SCG 基地址 ========== */
/* TODO §5.2：从 S32K144 参考手册中找到 SCG 寄存器基地址 */
#define SCG_BASE            (0x40064000u)

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
 *        配置 SOSC 为 8MHz 外部晶振模式
 *
 * @return 0=成功, -1=超时
 *
 * TODO §5.2.1：实现 SOSC 初始化
 *       关键寄存器：
 *         - SCG_SOSCCSR:  SOSC 控制和状态
 *         - SCG_SOSCDIV: SOSC 分频
 *         - SCG_SOSCCFG: SOSC 配置 (增益、模式)
 *
 *       步骤：
 *         1. 锁定 SCG_SOSCCSR[LK] 检查
 *         2. 写 SCG_SOSCCSR 使能 SOSC
 *            - SOSCEN = 1
 *            - SOSCMON = 0 (禁用监视)
 *         3. wait for SCG_SOSCCSR[SOSCVLD] 置位
 *         4. 配置分频: SCG_SOSCDIV[SOSCDIV1] [SOSCDIV2]
 */
int clock_init_sosc(void);

/**
 * @brief 初始化 SPLL (系统 PLL)
 *        使用外部晶振 (SOSC) 作为参考时钟
 *        目标输出: 160MHz
 *
 * @return 0=成功, -1=超时
 *
 * TODO §5.2.2：实现 SPLL 初始化
 *       关键寄存器：
 *         - SCG_SPLLCSR:  SPLL 控制和状态
 *         - SCG_SPLLDIV:  SPLL 分频
 *         - SCG_SPLLCFG:  SPLL 配置 (PREDIV, MULT, SPLLSEL)
 *
 *       公式：F_SPLL = F_REF * MULT / PREDIV
 *       PREDIV = 1, MULT = 40: F_SPLL = 8MHz * 40 / 1 = 320MHz
 *       注意：S32K144 SPLL 最大 160MHz，需要再看手册确认分频
 */
int clock_init_spll(void);

/**
 * @brief 选择系统时钟源
 *
 * @param[in]  source   系统时钟源选择
 * @return     0=成功, -1=参数无效或切换失败
 *
 * TODO §5.2.3：实现系统时钟源切换
 *       关键寄存器：
 *         - SCG_CSR: 时钟状态寄存器
 *           - SCS[2:0]: 系统时钟选择
 *
 *       步骤：
 *         1. 检查目标时钟源是否稳定 (VLD 标志)
 *         2. SCG_CSR[SCS] = source
 *         3. 等待 SCG_CSR[SCS] 确认切换完成
 */
int clock_set_sysclk(clock_source_t source);

/**
 * @brief 获取当前系统时钟频率
 *
 * @return 当前系统时钟频率 (Hz)
 *
 * TODO §5.2.4：实现时钟频率获取
 *       读取 SCG_CSR[SCS] 判断当前时钟源
 *       结合分频配置计算实际频率
 */
uint32_t clock_get_freq(void);

/**
 * @brief 配置外设时钟分频
 *        S32K144 有三条时钟总线：
 *        - CORE_CLK:  内核时钟
 *        - BUS_CLK:   总线时钟
 *        - SLOW_CLK:  慢速时钟
 *       各外设挂载在不同时钟总线上
 *
 * @param[in]  div_core   CORE 时钟分频
 * @param[in]  div_bus    BUS 时钟分频
 * @param[in]  div_slow   SLOW 时钟分频
 * @return     0=成功, -1=参数错误
 *
 * TODO §5.2.5：实现外设时钟分频
 *       关键寄存器：
 *         - SCG_CSR: 除系统时钟选择外，也包含分频配置
 */
int clock_set_periph_div(clock_div_t div_core,
                         clock_div_t div_bus,
                         clock_div_t div_slow);
