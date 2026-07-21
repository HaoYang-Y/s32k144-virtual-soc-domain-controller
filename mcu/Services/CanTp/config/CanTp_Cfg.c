/**
 * @file    CanTp_Cfg.c
 * @brief   CAN Transport Layer 配置实例 — N-PDU 通道映射表
 *
 * @note    每个 TP 通道对应一个 CanIf PDU：
 *          - 通道 0 = TX 0x123 (与 CanIf_PduConfig[0] 对应)
 *          - 通道 1 = RX 0x100 (与 CanIf_PduConfig[1] 对应)
 */

#include "CanTp_Cfg.h"

/** @brief N-PDU 通道配置表 */
const CanTp_NPduConfigType CanTp_NPduConfig[CANTP_CHANNEL_COUNT] = {
    /* TX 通道: TP PDU 0 → CanIf PDU 0 (CAN ID 0x123) */
    {
        .tp_pdu_id     = 0U,
        .canif_pdu_id  = 0U,         /* CANIF_PDU_ID_TX_0x123 */
        .controller_id = 0U,
        .can_id        = 0x123U,
        .dlc           = 8U,
    },
    /* RX 通道: TP PDU 1 → CanIf PDU 1 (CAN ID 0x100) */
    {
        .tp_pdu_id     = 1U,
        .canif_pdu_id  = 1U,         /* CANIF_PDU_ID_RX_0x100 */
        .controller_id = 0U,
        .can_id        = 0x100U,
        .dlc           = 8U,
    },
};

/** @brief 配置条目计数 */
const uint8_t CanTp_NPduConfig_Count = CANTP_CHANNEL_COUNT;