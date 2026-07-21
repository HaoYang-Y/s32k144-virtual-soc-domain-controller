/**
 * @file    CanTp_Cfg.h
 * @brief   CAN Transport Layer 配置 — N-PDU 通道 + 时序参数
 *
 * @note    ISO 15765-2 超时参数 + N-PDU 到 CanIf PDU 的映射表
 */

#ifndef CANTP_CFG_H
#define CANTP_CFG_H

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ===================================================================
 *  ISO 15765-2 超时参数 (ms)
 * =================================================================== */

#define CANTP_AS_TIMEOUT_MS        1000U   /* N_As: 发送确认超时 */
#define CANTP_AR_TIMEOUT_MS        1000U   /* N_Ar: 接收确认超时 */
#define CANTP_BS_TIMEOUT_MS        1000U   /* N_Bs: 接收方发送流控超时 */
#define CANTP_CR_TIMEOUT_MS        1000U   /* N_Cr: 连续帧到达超时 */

/* ===================================================================
 *  流控参数
 * =================================================================== */

#define CANTP_BLOCK_SIZE           8U      /* BS: 连续帧块大小 */
#define CANTP_STMIN_US             100U    /* STmin: 连续帧最小间隔 (us) */

/** @brief 最大单帧数据长度 (CAN TP SF payload = 7 bytes for classic CAN) */
#define CANTP_SF_MAX_DATA          7U

/** @brief 多帧接收缓冲区大小 (支持最大 256 bytes I-PDU) */
#define CANTP_RX_BUFFER_SIZE       256U

/* ===================================================================
 *  N-PDU 通道配置
 * =================================================================== */

/** @brief CAN TP 通道数 */
#define CANTP_CHANNEL_COUNT        2U

/** @brief N-PDU 通道配置: TP PDU ID → CanIf PDU ID + CAN ID */
typedef struct {
    uint16_t  tp_pdu_id;        /**< CanTp 内部通道 PDU ID */
    uint16_t  canif_pdu_id;     /**< 对应的 CanIf PDU ID (用于转发) */
    uint8_t   controller_id;    /**< CAN 控制器 ID */
    uint32_t  can_id;           /**< CAN 报文 ID */
    uint8_t   dlc;              /**< CAN 帧 DLC (固定 8) */
} CanTp_NPduConfigType;

/** @brief N-PDU 通道配置表 (由 signals.yaml 生成或手动配置) */
extern const CanTp_NPduConfigType CanTp_NPduConfig[];
extern const uint8_t              CanTp_NPduConfig_Count;

#endif /* CANTP_CFG_H */