/**
 * @file    Adc.h
 * @brief   [SKELETON] AUTOSAR MCAL Adc 驱动头文件
 *
 * @note    对应 AUTOSAR CP MCAL Adc 驱动程序规范
 *          基于 S32K144 ADC0 硬件，单次/连续扫描模式
 */

#ifndef MCAL_ADC_H
#define MCAL_ADC_H

#include "Std_Types.h"
#include "Adc_Cfg.h"

typedef uint8_t Adc_ChannelType;
typedef uint16_t Adc_ValueType;

typedef enum {
    ADC_MODE_SINGLE   = 0,
    ADC_MODE_CONTINUOUS = 1
} Adc_ConversionModeType;

typedef enum {
    ADC_IDLE    = 0,
    ADC_BUSY    = 1,
    ADC_COMPLETE = 2
} Adc_StatusType;

typedef struct {
    Adc_ChannelType   channel;
    Adc_ValueType     *result_ptr;
} Adc_ChannelConfigType;

typedef struct {
    const Adc_ChannelConfigType *channels;
    uint8                        num_channels;
    Adc_ConversionModeType       mode;
} Adc_ConfigType;

void Adc_Init(const Adc_ConfigType *ConfigPtr);
void Adc_DeInit(void);
void Adc_StartConversion(void);
void Adc_StopConversion(void);
Adc_StatusType Adc_GetStatus(void);
Adc_ValueType  Adc_ReadChannel(Adc_ChannelType Channel);

#endif /* MCAL_ADC_H */
