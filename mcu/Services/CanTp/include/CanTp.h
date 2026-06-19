/**
 * @file    CanTp.h
 * @brief   [SKELETON] CAN Transport Layer — AUTOSAR CP Services 层
 *
 * @note    多帧分包/重组 (ISO 15765-2)
 *          Single Frame → 直接发
 *          Multi Frame → FF + CF + FC 流控
 */

#ifndef CANTP_H
#define CANTP_H

#include "Std_Types.h"

typedef uint16_t CanTp_PduIdType;

typedef struct {
    uint8_t  *data;
    uint16_t  length;
} CanTp_PduType;

typedef enum {
    CANTP_SF = 0,  /* Single Frame */
    CANTP_FF = 1,  /* First Frame */
    CANTP_CF = 2,  /* Consecutive Frame */
    CANTP_FC = 3,  /* Flow Control */
} CanTp_FrameType;

void    CanTp_Init(void);
uint8_t CanTp_Transmit(CanTp_PduIdType TxPduId, const CanTp_PduType *PduInfo);
void    CanTp_RxIndication(CanTp_PduIdType RxPduId, const uint8_t *CanSdu, uint8_t Length);
void    CanTp_TxConfirmation(CanTp_PduIdType TxPduId);

#endif /* CANTP_H */
