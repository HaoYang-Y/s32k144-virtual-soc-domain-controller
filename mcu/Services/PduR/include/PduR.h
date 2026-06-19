/**
 * @file    PduR.h
 * @brief   [SKELETON] PDU Router — AUTOSAR CP Services 层
 *
 * @note    PDU 路由：在 Com ←→ CanIf (通过 CanTp) 之间路由 I-PDU
 *          支持 1:N 和 N:1 路由模式
 */

#ifndef PDUR_H
#define PDUR_H

#include "Std_Types.h"

typedef uint16_t PduR_PduIdType;

typedef struct {
    uint16_t  id;
    uint8_t   length;
    uint8_t   *data;
} PduR_InfoType;

void    PduR_Init(void);
uint8_t PduR_ComTransmit(PduR_PduIdType PduId, const PduR_InfoType *PduInfo);
void    PduR_CanIfRxIndication(PduR_PduIdType RxPduId, const PduR_InfoType *PduInfo);
void    PduR_CanIfTxConfirmation(PduR_PduIdType TxPduId);

#endif /* PDUR_H */
