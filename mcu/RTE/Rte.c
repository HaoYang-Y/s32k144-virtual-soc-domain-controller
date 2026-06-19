/**
 * @file    Rte.c
 * @brief   [SKELETON] Runtime Environment 实现
 *
 * @todo    实现 SWC↔BSW 桥接和信号映射表
 */

#include "Rte.h"

void Rte_Init(void)         { /* TODO */ }
void Rte_Start(void)        { /* TODO */ }
void Rte_MainFunction(void) { /* TODO */ }

void Rte_Write_VehicleSignal(uint8_t SignalId, const uint8_t *Data, uint8_t Length)
{
    (void)SignalId;
    (void)Data;
    (void)Length;
    /* TODO: 查找信号映射表 → 调用 Com_SendSignal */
}

void Rte_Read_VehicleSignal(uint8_t SignalId, uint8_t *Data, uint8_t *Length)
{
    (void)SignalId;
    (void)Data;
    (void)Length;
    /* TODO: 调用 Com_ReceiveSignal */
}
