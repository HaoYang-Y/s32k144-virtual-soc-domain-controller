/**
 * @file    CanIf_PduId.h
 * @brief   [AUTOSAR CP] CanIf 配置 — 由 generate_canif_cfg.py 自动生成
 *
 * @note    数据源: mcu/config/signals.yaml
 *          不要手改此文件 — 改 YAML 后重新运行生成脚本
 */

#ifndef CANIF_PDUID_H
#define CANIF_PDUID_H

/* ===================================================================
 *  PDU ID 宏定义（对标 AUTOSAR CanIf_PduIdType）
 * =================================================================== */

/** @brief TX PDU — CAN ID 0x123 */
#define CANIF_PDU_ID_TX_0x123                    0U

/** @brief RX PDU — CAN ID 0x100 */
#define CANIF_PDU_ID_RX_0x100                    1U

/** @brief 已配置的 PDU 总数 */
#define CANIF_PDU_COUNT  2U

#endif /* CANIF_PDUID_H */
