/**
 * @file    CanIf_Cfg.c
 * @brief   [AUTOSAR CP] CanIf 配置 — 由 generate_canif_cfg.py 自动生成
 *
 * @note    数据源: mcu/config/signals.yaml
 *          不要手改此文件 — 改 YAML 后重新运行生成脚本
 */

#include "CanIf_Cfg.h"
#include "CanIf_PduId.h"
#include "Can.h"   /* CAN_HTH_MAKE — TX 条目 hth 字段 */

/* ===================================================================
 *  CanIf PDU 配置表（对标 AUTOSAR CanIf_PduConfigType）
 *
 *  每个 PDU 对应一条 CAN 报文（group by can_id + direction）
 *  CanIf_Transmit() / CanIf_RxIndication() 据此查表
 * =================================================================== */

const CanIf_PduConfigType CanIf_PduConfig[CANIF_PDU_COUNT] = {
    {CANIF_PDU_ID_TX_0x123, 0U, 0x00000123UL, 8U, CAN_HTH_MAKE(0U, 0U)},  /* TX: CAN ID 0x123 */
    {CANIF_PDU_ID_RX_0x100, 0U, 0x00000100UL, 8U, 0U},  /* RX: CAN ID 0x100 */
};

const uint8_t CanIf_PduConfig_Count = CANIF_PDU_COUNT;
