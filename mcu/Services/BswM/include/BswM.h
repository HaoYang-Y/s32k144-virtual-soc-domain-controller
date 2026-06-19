/**
 * @file    BswM.h
 * @brief   [SKELETON] BSW Mode Manager — AUTOSAR CP Services 层
 *
 * @note    模式仲裁: 根据请求仲裁通信模式、ECU 模式
 *          如: 正常运行模式、诊断模式、休眠模式等
 */

#ifndef BSWM_H
#define BSWM_H

#include "Std_Types.h"

void BswM_Init(void);
void BswM_MainFunction(void);

void BswM_RequestComMode(uint8_t ControllerId, uint8_t ComMode);
void BswM_RequestEcuMode(uint8_t EcuMode);
void BswM_RequestEcuWakeup(uint8_t WakeupSource);

#endif /* BSWM_H */
