/**
 * @file    Com.c
 * @brief   [SKELETON] Communication 模块实现
 *
 * @todo    实现信号↔PDU 映射表，周期性调用 Com_MainFunction 发送周期消息
 */

#include "Com.h"
/* TODO: #include "PduR.h" */

void Com_Init(void) { /* TODO */ }
void Com_MainFunction(void) { /* TODO */ }

void Com_SendSignal(Com_SignalIdType SignalId, const void *SignalData)
{
    (void)SignalId;
    (void)SignalData;
    /* TODO: 编码信号到 I-PDU → 调用 PduR_ComTransmit */
}

void Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalData)
{
    (void)SignalId;
    (void)SignalData;
    /* TODO: 从 I-PDU 解码出信号值 */
}

uint8_t Com_SendSignalGroup(Com_SignalIdType GroupId)
{
    (void)GroupId;
    return 0U;
}

uint8_t Com_ReceiveSignalGroup(Com_SignalIdType GroupId)
{
    (void)GroupId;
    return 0U;
}
