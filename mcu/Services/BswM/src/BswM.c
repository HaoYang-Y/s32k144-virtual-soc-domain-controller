/**
 * @file    BswM.c
 * @brief   [SKELETON] BSW Mode Manager 实现
 */

#include "BswM.h"

void BswM_Init(void)          { /* TODO */ }
void BswM_MainFunction(void)  { /* TODO */ }

void BswM_RequestComMode(uint8_t ControllerId, uint8_t ComMode)
{
    (void)ControllerId;
    (void)ComMode;
    /* TODO: 仲裁后通知 ComM */
}

void BswM_RequestEcuMode(uint8_t EcuMode)
{
    (void)EcuMode;
    /* TODO: 仲裁后通知 EcuM */
}

void BswM_RequestEcuWakeup(uint8_t WakeupSource)
{
    (void)WakeupSource;
    /* TODO */
}
