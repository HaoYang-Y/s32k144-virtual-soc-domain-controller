/**
 * @file    PduR.c
 * @brief   [AUTOSAR CP] PDU Router 实现
 *
 * @note    TODO: 路由表查找 → 根据 PduId 转发至目标模块
 */

#include "PduR.h"

void PduR_Init(void) { /* TODO */ }

Std_ReturnType PduR_ComTransmit(PduIdType PduId, const PduInfoType *PduInfoPtr)
{
    (void)PduId;
    (void)PduInfoPtr;
    /* TODO: 查找路由表 → 调用 CanTp_Transmit 或 CanIf_Transmit */
    return E_OK;
}

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    (void)RxPduId;
    (void)PduInfoPtr;
    /* TODO: 查找路由表 → 调用 Com_RxIndication(路由后的 I-PDU ID, PduInfoPtr) */
}

void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    (void)TxPduId;
    /* TODO: 通知 Com 发送完成 */
}