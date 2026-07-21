/**
 * @file    PduR.h
 * @brief   [AUTOSAR CP] PDU Router — Services 层
 *
 * @note    对标 AUTOSAR SWS_PduR:
 *          - PduR 使用 ComStack_Types.h 中的 PduInfoType，不定义自己的 PDU 包装类型
 *          - 路由 I-PDU 在 Com ↔ CanTp/CanIf 之间
 *          - 支持 1:N 和 N:1 路由模式
 */

#ifndef PDUR_H
#define PDUR_H

#include "Std_Types.h"
#include "ComStack_Types.h"        /* PduInfoType */

void           PduR_Init(void);

/** @brief Com → PduR → CanTp/CanIf 发送路径 */
Std_ReturnType PduR_ComTransmit(PduIdType PduId, const PduInfoType *PduInfoPtr);

/** @brief CanIf → PduR → Com 接收指示 */
void           PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/** @brief CanIf → PduR → Com 发送确认 */
void           PduR_CanIfTxConfirmation(PduIdType TxPduId);

#endif /* PDUR_H */