/**
 * @file    IoHwAb.h
 * @brief   [SKELETON] I/O 硬件抽象层 — AUTOSAR CP ECU Abstraction
 *
 * @note    封装 MCAL Gpio / Adc 驱动，为 SWC 提供物理信号值
 *          如: 读取按钮状态、读取电位计电压值
 */

#ifndef IOHWAB_H
#define IOHWAB_H

#include "Std_Types.h"

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief 信号 ID */
typedef uint8_t IoHwAb_SignalIdType;

/** @brief I/O 信号类型 */
typedef enum {
    IOHWAB_DIGITAL_IN  = 0,
    IOHWAB_DIGITAL_OUT = 1,
    IOHWAB_ANALOG_IN   = 2,
} IoHwAb_SignalType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

void    IoHwAb_Init(void);
uint8_t IoHwAb_ReadDigital(IoHwAb_SignalIdType SignalId);
void    IoHwAb_WriteDigital(IoHwAb_SignalIdType SignalId, uint8_t Level);
uint16_t IoHwAb_ReadAnalog(IoHwAb_SignalIdType SignalId);

#endif /* IOHWAB_H */
