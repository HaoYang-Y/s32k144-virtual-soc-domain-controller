/**
 * @file    Adc.c
 * @brief   [SKELETON] AUTOSAR MCAL Adc 驱动实现
 *
 * @todo    基于 S32K144 ADC0 硬件，实现单次/连续转换
 */

#include "Adc.h"

static const Adc_ConfigType *Adc_ConfigPtr = NULL_PTR;
static Adc_StatusType         Adc_Status   = ADC_IDLE;

void Adc_Init(const Adc_ConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) return;
    Adc_ConfigPtr = ConfigPtr;
    /* TODO: 初始化 ADC0 硬件 (时钟、分辨率、触发源) */
    Adc_Status = ADC_IDLE;
}

void Adc_DeInit(void)
{
    Adc_Status = ADC_IDLE;
    Adc_ConfigPtr = NULL_PTR;
}

void Adc_StartConversion(void)
{
    /* TODO: 启动 ADC 转换 */
    Adc_Status = ADC_BUSY;
}

void Adc_StopConversion(void)
{
    /* TODO: 停止 ADC */
    Adc_Status = ADC_IDLE;
}

Adc_StatusType Adc_GetStatus(void)
{
    return Adc_Status;
}

Adc_ValueType Adc_ReadChannel(Adc_ChannelType Channel)
{
    (void)Channel;
    /* TODO: 读取指定通道的转换结果 */
    return 0U;
}
