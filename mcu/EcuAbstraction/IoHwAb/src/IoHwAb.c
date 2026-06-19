/**
 * @file    IoHwAb.c
 * @brief   [SKELETON] I/O 硬件抽象层实现
 *
 * @todo    绑定 MCAL Gpio / Adc 驱动，提供信号级读写
 */

#include "IoHwAb.h"
/* TODO: #include "Gpio.h"   #include "Adc.h" */

void IoHwAb_Init(void)
{
    /* TODO */
}

uint8_t IoHwAb_ReadDigital(IoHwAb_SignalIdType SignalId)
{
    (void)SignalId;
    /* TODO: 调用 Gpio_ReadPin */
    return 0U;
}

void IoHwAb_WriteDigital(IoHwAb_SignalIdType SignalId, uint8_t Level)
{
    (void)SignalId;
    (void)Level;
    /* TODO: 调用 Gpio_WritePin */
}

uint16_t IoHwAb_ReadAnalog(IoHwAb_SignalIdType SignalId)
{
    (void)SignalId;
    /* TODO: 调用 Adc_ReadChannel */
    return 0U;
}
