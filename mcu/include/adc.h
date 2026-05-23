/*
 * @brief ADC 驱动头文件 (S32K144)
 *        封装基于 NXP S32 SDK 的模数转换功能
 *
 * @note 本驱动使用 NXP S32 SDK ADC_DRV API 实现
 *       涉及的头文件：adc_driver.h
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== 枚举定义 ========== */

/** @brief ADC 通道 */
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
    ADC_REF_VALT = 1,    /**< 备用参考电压 */
} adc_ref_sel_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化 ADC
 *        通过 NXP S32 SDK 配置 ADC 时钟、分辨率和采样时间
 *
 * @param[in]  resolution    ADC 分辨率 (8/10/12 bit)
 * @param[in]  ref_sel       参考电压选择
 * @return     0=成功, -1=参数错误
 *
 * @note SDK API: ADC_DRV_Init()
 *       配置结构体: adc_user_config_t
 */
int adc_init(adc_resolution_t resolution, adc_ref_sel_t ref_sel);

/**
 * @brief 启动 ADC 转换并读取结果（阻塞）
 *        单次转换模式
 *
 * @param[in]  channel   ADC 通道 (0~15)
 * @return     转换结果 (原始 ADC 值)
 *
 * @note SDK API: ADC_DRV_StartConv() + ADC_DRV_GetConvResult()
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
 * @note 公式: voltage = raw_value * vref_mv / (2^resolution - 1)
 */
uint32_t adc_raw_to_mv(uint16_t raw_value,
                       adc_resolution_t resolution,
                       uint32_t vref_mv);

/**
 * @brief 获取最后一次转换的结果（不触发新转换）
 * @return  ADC 结果寄存器的值
 *
 * @note SDK API: ADC_DRV_GetConvResult()
 */
uint16_t adc_get_last_result(void);
