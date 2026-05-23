/*
 * @brief ADC 驱动头文件 (S32K144)
 *        封装 ADC 模块的寄存器操作，提供模数转换功能
 *
 * @note 学习目标：理解 ADC 逐次逼近转换原理
 *       对应书籍：第10章 ADC 模块
 *
 * TODO：看书后实现以下函数
 *       1. adc_init()       — 初始化 ADC 时钟、分辨率、采样时间
 *       2. adc_read()       — 启动一次转换并读取结果（阻塞）
 *       3. adc_read_vol()   — 将转换结果转为电压值（mV）
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== ADC 基地址 ========== */
/* TODO §10.2：从 S32K144 参考手册中找到 ADC 寄存器基地址 */
#define ADC0_BASE       (0x4003B000u)
#define ADC1_BASE       (0x40027000u)

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
 *        1. 使能 ADC 时钟 (PCC_ADC[CGC])
 *        2. 复位 ADC
 *        3. 配置转换模式 (SC1n/SC2/SC3)
 *        4. 配置分辨率 (CFG1[MODE])
 *        5. 配置采样时间 (CFG1[ADLSMP], CFG2)
 *
 * @param[in]  resolution    ADC 分辨率 (8/10/12 bit)
 * @param[in]  ref_sel       参考电压选择
 * @return     0=成功, -1=参数错误
 *
 * TODO §10.2.1：实现 ADC 初始化
 *       关键寄存器：
 *         - SC1n: ADC 通道选择和中断配置
 *         - CFG1: MODE 位控制分辨率
 *         - CFG2: 采样时间配置
 *         - SC2:  触发模式、参考电压
 *         - SC3:  连续转换/硬件平均
 */
int adc_init(adc_resolution_t resolution, adc_ref_sel_t ref_sel);

/**
 * @brief 启动 ADC 转换并读取结果（阻塞）
 *        单次转换模式
 *
 * @param[in]  channel   ADC 通道 (0~15)
 * @return     转换结果 (原始 ADC 值)
 *
 * TODO：实现单次 ADC 转换
 *       1. 等待 SC1n[COCO] 标志清零（上次转换完成）
 *       2. 写 SC1n[ADCH] 选择通道
 *       3. 等待 SC1n[COCO] 置位（本次转换完成）
 *       4. 读取 Rn 结果寄存器
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
 * TODO：实现电压转换
 *       公式: voltage = raw_value * vref_mv / (2^resolution - 1)
 */
uint32_t adc_raw_to_mv(uint16_t raw_value,
                       adc_resolution_t resolution,
                       uint32_t vref_mv);

/**
 * @brief 获取最后一次转换的结果（不触发新转换）
 * @return  ADC 结果寄存器的值
 *
 * TODO：直接读取 Rn 结果寄存器
 */
uint16_t adc_get_last_result(void);
