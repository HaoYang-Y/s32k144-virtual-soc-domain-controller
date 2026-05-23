/*
 * @brief ADC 驱动实现 (S32K144)
 *        基于 NXP S32 SDK ADC_DRV API 实现模数转换
 *
 * @note 本驱动封装 NXP S32 SDK 的 ADC 驱动层 API
 *
 *       SDK API 涉及:
 *         - ADC_DRV_Init():             初始化 ADC 模块
 *         - ADC_DRV_StartConv():        启动 ADC 转换
 *         - ADC_DRV_GetConvResult():    获取转换结果
 *
 *       SDK 配置结构体:
 *         - adc_user_config_t:  包含分辨率、参考电压、采样时间等
 *         - adc_conv_result_t:  转换结果结构体
 */
#include "adc.h"

/* NXP S32 SDK ADC 驱动头文件 */
#include "adc_driver.h"

/* ========== 函数实现 ========== */

int adc_init(adc_resolution_t resolution, adc_ref_sel_t ref_sel)
{
    adc_user_config_t adcConfig;

    /* 参数检查 */
    if ((resolution > ADC_RES_12BIT) || (ref_sel > ADC_REF_VALT))
    {
        return -1;
    }

    /*
     * 使用 SDK 默认配置初始化 adc_user_config_t
     * SDK 提供 ADC_DRV_GetDefaultConfig() 填充默认值
     */
    ADC_DRV_GetDefaultConfig(&adcConfig);

    /* 根据参数配置 ADC */
    adcConfig.resolution   = (uint8_t)resolution;
    adcConfig.refVoltSrc   = (uint8_t)ref_sel;
    adcConfig.clockDivider = 0u;       /* 时钟分频: 1 */
    adcConfig.sampleTime   = 0u;       /* 短采样时间 */
    adcConfig.longSample   = false;    /* 短采样模式 */
    adcConfig.hwAverage    = false;    /* 禁止硬件平均 */
    adcConfig.averageNum   = 0u;       /* 平均次数: 无 */
    adcConfig.triggerSource = 0u;      /* 软件触发 */
    adcConfig.enableDma    = false;    /* 禁止 DMA */
    adcConfig.dmaChnl      = 0u;       /* DMA 通道: 无 */

    /*
     * 调用 SDK API 初始化 ADC 模块
     * SDK 内部自动完成:
     *   1. 使能 PCC_ADC0 时钟
     *   2. 配置 CFG1 寄存器 (MODE, ADICLK, ADIV, ADLSMP)
     *   3. 配置 CFG2 寄存器 (SMPLTS)
     *   4. 配置 SC2 寄存器 (REFSEL, ADTRG)
     *   5. 校准 ADC
     */
    ADC_DRV_Init(0u, &adcConfig);

    return 0;
}

uint16_t adc_read(adc_channel_t channel)
{
    adc_conv_result_t result;

    /* 参数检查 */
    if (channel > ADC_CHANNEL_15)
    {
        return 0u;
    }

    /*
     * 调用 SDK API 启动 ADC 转换并等待完成
     *
     * SDK 内部自动完成:
     *   1. 配置 SC1n[ADCH] = channel 选择通道
     *   2. 等待 COCO 标志位置位 (转换完成)
     *   3. 读取 Rn 结果寄存器
     *   4. 将 MB 重新设置为空闲状态
     */
    (void)ADC_DRV_StartConv(0u, (uint8_t)channel);
    ADC_DRV_GetConvResult(0u, &result);

    return (uint16_t)result.result;
}

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
            max_value = 255u;    /* 2^8 - 1 */
            break;

        case ADC_RES_10BIT:
            max_value = 1023u;   /* 2^10 - 1 */
            break;

        case ADC_RES_12BIT:
            max_value = 4095u;   /* 2^12 - 1 */
            break;

        default:
            return 0u;
    }

    /*
     * 电压转换公式:
     * voltage = raw_value * vref_mv / max_value
     *
     * 先乘后除以保持精度，使用 uint32_t 避免溢出
     */
    voltage_mv = ((uint32_t)raw_value * vref_mv) / max_value;

    return voltage_mv;
}

uint16_t adc_get_last_result(void)
{
    adc_conv_result_t result;

    /*
     * 调用 SDK API 获取最后一次转换结果
     * 不触发新转换
     */
    ADC_DRV_GetConvResult(0u, &result);

    return (uint16_t)result.result;
}
