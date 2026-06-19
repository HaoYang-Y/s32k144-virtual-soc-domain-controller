/**
 * @file    CanTp.c
 * @brief   [SKELETON] CAN Transport Layer 实现
 */

#include "CanTp.h"

void CanTp_Init(void) { /* TODO */ }

uint8_t CanTp_Transmit(CanTp_PduIdType TxPduId, const CanTp_PduType *PduInfo)
{
    (void)TxPduId;
    (void)PduInfo;
    /* TODO: 判断 SF/FF → 发送给 CanIf */
    return 0U;
}

void CanTp_RxIndication(CanTp_PduIdType RxPduId, const uint8_t *CanSdu, uint8_t Length)
{
    (void)RxPduId;
    (void)CanSdu;
    (void)Length;
    /* TODO: 判断 SF/FF → 收到全部后重组 → 转发给 PduR */
}

void CanTp_TxConfirmation(CanTp_PduIdType TxPduId)
{
    (void)TxPduId;
    /* TODO */
}
