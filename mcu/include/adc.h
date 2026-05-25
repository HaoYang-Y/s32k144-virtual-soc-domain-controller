/*
 * @brief ADC 驱动头文件 (S32K144)
 *        通过直接操作寄存器实现模数转换
 *
 * @note 参考: S32K1xx Reference Manual, Chapter 54 (ADC)
 *       本驱动使用寄存器操作方式，不依赖 SDK 的 adc_driver
 *
 *       关键寄存器：
 *         - SC1n: ADC 状态和控制寄存器 n (n=0~7)
 *         - CFG1: 配置寄存器 1 (MODE, ADICLK, ADIV, ADLSMP)
 *         - CFG2: 配置寄存器 2 (SMPLTS)
 *         - Rn:   ADC 数据结果寄存器 n
 *         - SC2:  状态和控制寄存器 2 (REFSEL, ADTRG)
 *         - SC3:  状态和控制寄存器 3 (AVGE, AVGS)
 *         - PG:   增益寄存器 (校准用)
 *         - CLPS: 偏移校准信号寄存器 (校准用)
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== ADC 模块基地址 ========== */
#define ADC0_BASE       (0x4003B000u)

/* ========== 枚举定义 ========== */

/** @brief ADC 物理通道号 (0~15 对应 AD0~AD15) */
typedef enum {
    ADC_CHANNEL_0  = 0,
    ADC_CHANNEL_1  = 1,
    ADC_CHANNEL_2  = 2,
    ADC_CHANNEL_3  = 3,
    ADC_CHANNEL_4  = 4,
    ADC_CHANNEL_5  = 5,
    ADC_CHANNEL_6  = 6,
    ADC_CHANNEL_7  = 7,
    ADC_CHANNEL_8  = 8,
    ADC_CHANNEL_9  = 9,
    ADC_CHANNEL_10 = 10,
    ADC_CHANNEL_11 = 11,
    ADC_CHANNEL_12 = 12,
    ADC_CHANNEL_13 = 13,
    ADC_CHANNEL_14 = 14,
    ADC_CHANNEL_15 = 15,
} adc_channel_t;

/** @brief ADC 分辨率 */
typedef enum {
    ADC_RES_8BIT  = 0,   /**< 8 位分辨率 */
    ADC_RES_10BIT = 1,   /**< 10 位分辨率 */
    ADC_RES_12BIT = 2,   /**< 12 位分辨率 (默认) */
} adc_resolution_t;

/** @brief ADC 参考电压源 */
typedef enum {
    ADC_REF_VREF = 0,    /**< 外部 VREFH/VREFL 引脚 */
    ADC_REF_VALT = 1,    /**< 备用参考电压 (VALT) */
} adc_ref_sel_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化 ADC0 模块
 *        通过寄存器操作完成时钟使能、校准和基本配置
 *
 * @param[in]  resolution    ADC 分辨率 (ADC_RES_8BIT/10BIT/12BIT)
 * @param[in]  ref_sel       参考电压选择 (ADC_REF_VREF/ALT)
 * @return     0=成功, -1=参数错误
 *
 * @note 寄存器操作步骤:
 *       1. 使能 PCC_ADC0 时钟 (PCC_ADC0[CGC]=1)
 *       2. 配置 CFG1[MODE] 分辨率
 *       3. 配置 CFG1[ADICLK] 输入时钟选择
 *       4. 配置 CFG1[ADIV] 分频系数
 *       5. 配置 SC2[REFSEL] 参考电压
 *       6. 执行自校准流程
 */
int adc_init(adc_resolution_t resolution, adc_ref_sel_t ref_sel);

/**
 * @brief 启动 ADC 转换并读取结果（阻塞模式）
 *        单次转换，自动选择 SC1[0] 寄存器
 *
 * @param[in]  channel   ADC 通道 (0~15)
 * @return     转换结果 (原始 ADC 值)，错误返回 0
 *
 * @note 操作流程:
 *       1. 写入 SC1[0] 选择通道 (ADCH 位域)
 *       2. 轮询等待 COCO 标志位置位
 *       3. 读取 R[0] 数据结果寄存器
 *       4. 自动清除 COCO 标志
 */
uint16_t adc_read(adc_channel_t channel);

/**
 * @brief 将 ADC 原始值转换为电压值
 *
 * @param[in]  raw_value    ADC 原始转换值
 * @param[in]  resolution   ADC 分辨率
 * @param[in]  vref_mv      参考电压 (mV)，通常为 3300
 * @return     电压值 (mV)
 *
 * @note 公式: voltage = raw_value * vref_mv / (2^bits - 1)
 */
uint32_t adc_raw_to_mv(uint16_t raw_value,
                       adc_resolution_t resolution,
                       uint32_t vref_mv);

/**
 * @brief 读取上一次转换结果（不触发新转换）
 * @return  最后一次转换的原始值
 */
uint16_t adc_get_last_result(void);
