/**
 * @file    PduR.c
 * @brief   [SKELETON] PDU Router 实现
 */

#include "PduR.h"

void PduR_Init(void) { /* TODO */ }

uint8_t PduR_ComTransmit(PduR_PduIdType PduId, const PduR_InfoType *PduInfo)
{
    (void)PduId;
    (void)PduInfo;
    /* TODO: 查找路由表 → 调用 CanIf_Transmit(目标控制器, PDU) */
    return 0U;
}

void PduR_CanIfRxIndication(PduR_PduIdType RxPduId, const PduR_InfoType *PduInfo)
{
    (void)RxPduId;
    (void)PduInfo;
    /* TODO: 查找路由表 → 调用 Com_RxIndication */
}

void PduR_CanIfTxConfirmation(PduR_PduIdType TxPduId)
{
    (void)TxPduId;
    /* TODO: 通知 Com 发送完成 */
}
