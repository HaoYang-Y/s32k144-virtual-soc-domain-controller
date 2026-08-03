/**
 * @file    PduR.c
 * @brief   [AUTOSAR CP] PDU Router 实现 — Services 层
 *
 * @note    路由 I-PDU 在 Com ↔ CanTp/CanIf 之间。
 *          当前阶段: Com 未实现，CanTp 是唯一的 Service 层消费者。
 *          路由策略: 所有 PDU 先经过 CanTp (SF 直接透传，MF 分段/重组)。
 *          后续 Com 实现后，通过路由表决定走 CanTp 还是直达 CanIf。
 */

#include "PduR.h"
#include "CanTp.h"
#include "Com.h"
#include "Log.h"

void PduR_Init(void)
{
    LOG_I("PduR", "Init done");
}

/**
 * @brief Com → PduR → CanTp/CanIf 发送路径
 *
 * @note  当前所有 I-PDU 统一走 CanTp:
 *        - ≤7 bytes: CanTp 以 SF 发送 (一次 CanIf_Transmit)
 *        - >7 bytes: CanTp 以 FF+CF 分段发送 (FC 流控由 MainFunction 驱动)
 *        后续引入路由表后可区分 TP PDU 和 Direct PDU。
 */
Std_ReturnType PduR_ComTransmit(PduIdType PduId, const PduInfoType *PduInfoPtr)
{
    LOG_D("PduR", "ComTransmit: PduId=%u, len=%u",
          (unsigned int)PduId, (unsigned int)PduInfoPtr->SduLength);
    return CanTp_Transmit(PduId, PduInfoPtr);
}

/**
 * @brief CanIf → PduR → CanTp 接收指示
 *
 * @note  CanIf 收到 CAN 帧后调用。PduR 将原始 N-PDU (含 PCI) 交给 CanTp。
 *        CanTp 负责 PCI 解码 + 重组，完成后通过 PduR_CanTpRxIndication 向上交付。
 */
void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    LOG_D("PduR", "CanIfRxIndication: PduId=%u, len=%u",
          (unsigned int)RxPduId, (unsigned int)PduInfoPtr->SduLength);
    CanTp_RxIndication(RxPduId, PduInfoPtr);
}

/**
 * @brief CanTp → PduR → Com: 重组完成后的 I-PDU 向上交付
 *
 * @note  CanTp 完成 N-PDU 重组后调用 (SF 直接透传 / MF 全部 CF 收完)。
 *        I-PDU 不含 PCI 字节，是纯应用数据。
 *        后续 Com 实现后，此处 Route 到 Com_RxIndication。
 */
void PduR_CanTpRxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    LOG_D("PduR", "CanTpRxIndication: PduId=%u, len=%u (I-PDU ready for Com)",
          (unsigned int)RxPduId, (unsigned int)PduInfoPtr->SduLength);
    /* 路由到 COM 层: 解包 I-PDU → 更新信号 Shadow Buffer */
    Com_RxIndication(RxPduId, PduInfoPtr);
}

/**
 * @brief CanIf → PduR → CanTp: N-PDU 发送完成确认
 *
 * @note  Can 驱动完成一帧发送后，经 CanIf_TxConfirmation 到达此处。
 *        当前无路由表，直接路由到 CanTp (N-PDU 级确认，由 CanTp 聚合为
 *        I-PDU 级确认后向上通知)。后续 Com 实现后按路由表分发。
 */
void PduR_CanIfTxConfirmation(PduIdType TxPduId)
{
    LOG_D("PduR", "CanIfTxConfirmation: PduId=%u", (unsigned int)TxPduId);
    CanTp_TxConfirmation(TxPduId);
}

/**
 * @brief CanTp → PduR → Com: I-PDU 发送完成确认
 *
 * @note  CanTp 完成整个 I-PDU (SF 单帧 / MF 全部分段) 发送后调用。
 *        当前 Com 未实现，仅记录日志；后续路由到 Com_TxConfirmation。
 */
void PduR_CanTpTxConfirmation(PduIdType TxPduId)
{
    LOG_D("PduR", "CanTpTxConfirmation: PduId=%u (I-PDU sent)",
          (unsigned int)TxPduId);
    /* 路由到 COM 层: 通知 I-PDU 发送完成 */
    Com_TxConfirmation(TxPduId);
}
