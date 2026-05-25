/*
 * @brief ADC驱动实现 (S32K144)
 *
 * @note 基于NXP S32 SDK ADC_DRV API实现模数转换
 *       SDK API参考:
 *         - ADC_DRV_Init()         初始化ADC模块
 *         - ADC_DRV_StartConv()    启动ADC转换
 *         - ADC_DRV_GetChanResult() 获取通道转换结果
 *       SDK结构体:
 *         - adc_user_config_t      包含分辨率、参考电压、采样时间等
 *         - adc_conv_result_t      转换结果结构体
 */
#include "adc.h"

/* NXP S32 SDK ADC驱动头文件 */
#include "adc_driver.h"

/* -------------------------------------------------------------------------- */
/* 函数实现                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief 初始化ADC模块
 *
 * @param instance    ADC实例号 (0-1)
 * @param resolution  分辨率 (8/10/12位)
 * @param ref_sel     参考电压选择 (VREFH/VALTH)
 * @return            0成功, -1失败
 */
int adc_init(adc_resolution_t resolution, adc_ref_sel_t ref_sel)
{
    adc_user_config_t adcConfig;

    /* 参数检查 */
    if ((resolution > ADC_RES_12BIT) || (ref_sel > ADC_REF_VREFH))
    {
        return -1;
    }

    /* 使用SDK提供的默认配置 */
    ADC_DRV_GetDefaultConfig(&adcConfig);

    /* 根据参数配置ADC */
    adcConfig.resolution    = (adc_resolution_t)resolution;
    adcConfig.refSel        = (adc_ref_sel_t)ref_sel;
    adcConfig.clockDivider  = 0u;       /* 时钟分频: 1 */
    adcConfig.sampleTime    = (uint8_t)resolution;  /* 短采样时间 */
    adcConfig.longSample    = false;    /* 短采样模式 */
    adcConfig.hwAverageCtrl = false;    /* 禁止硬件平均 */
    adcConfig.averageNum    = 0u;       /* 平均次数: 无 */
    adcConfig.triggerSource = 0u;       /* 软件触发 */
    adcConfig.enableDma     = false;    /* 禁止DMA */
    adcConfig.dmaChnl       = 0u;       /* DMA通道: 无 */

    /*
     * 调用SDK API初始化ADC模块
     * SDK内部自动完成:
     *   1. 使能PCC模块时钟
     *   2. 配置CFG1寄存器 (MODE, ADICLK, ADIV, SM)
     *   3. 配置CFG2寄存器 (SMPLTS)
     *   4. 配置SC2寄存器 (REFSEL, ADTRG)
     *   5. 执行自校准
     */
    ADC_DRV_Init(0u, &adcConfig);

    return 0;
}

/**
 * @brief 读取指定通道的ADC转换值
 *
 * @param channel ADC通道号 (0-15)
 * @return        12位转换结果 (0-4095), 错误返回0
 */
uint16_t adc_read(adc_channel_t channel)
{
    adc_conv_result_t result;

    /* 参数检查 */
    if (channel > ADC_CHANNEL_15)
    {
        return 0u;
    }

    /*
     * 调用SDK API启动ADC转换并等待完成
     * SDK内部自动完成:
     *   1. 配置SC1n[ADCH] = channel 选择通道
     *   2. 等待COCO标志位置位 (转换完成)
     *   3. 读取Rn结果寄存器
     *   4. 将MB重新设置为空闲状态
     */
    (void)ADC_DRV_StartConv(0u, (uint8_t)channel);
    ADC_DRV_GetChanResult(0u, (uint8_t)channel, &result);

    return (uint16_t)result.result;
}

/**
 * @brief 将ADC原始值转换为毫伏电压
 *
 * @param raw_value  ADC原始值 (0-4095)
 * @param resolution ADC分辨率
 * @param vref_mv    参考电压 (毫伏)
 * @return           毫伏电压值, 错误返回0
 */
uint32_t adc_raw_to_mv(uint16_t raw_value,
                       adc_resolution_t resolution,
                       uint32_t vref_mv)
{
    uint32_t max_value;
    uint32_t voltage_mv;

    /* 根据分辨率计算最大值 */
    switch (resolution)
    {
        case ADC_RES_8BIT:
            max_value = 255u;
            break;

        case ADC_RES_10BIT:
            max_value = 1023u;
            break;

        case ADC_RES_12BIT:
            max_value = 4095u;
            break;

        default:
            return 0u;
    }

    /*
     * 电压转换公式:
     *   voltage = raw_value * vref_mv / max_value
     * 先乘后除以保持精度, 使用uint32_t避免溢出
     */
    voltage_mv = ((uint32_t)raw_value * vref_mv) / max_value;

    return voltage_mv;
}

/**
 * @brief 获取最后一次ADC转换结果 (不触发新转换)
 *
 * @return 12位转换结果 (0-4095)
 */
uint16_t adc_get_last_result(void)
{
    adc_conv_result_t result;
    uint8_t last_channel = 0u;

    /*
     * 调用SDK API获取最后一次转换结果
     * 不触发新转换, channel参数可任意填
     */
    ADC_DRV_GetChanResult(0u, last_channel, &result);

    return (uint16_t)result.result;
}
