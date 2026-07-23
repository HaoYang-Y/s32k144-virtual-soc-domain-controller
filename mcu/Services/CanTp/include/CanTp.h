/**
 * @file    CanTp.h
 * @brief   [AUTOSAR CP] CAN Transport Layer — Services 层
 *
 * @note    对标 AUTOSAR SWS_CanTp + ISO 15765-2:
 *          - CanTp 使用 ComStack_Types.h 中的 PduInfoType，不定义自己的 PDU 包装类型
 *          - 将上层 I-PDU 分段为 N-PDU (SF/FF/CF) → CanIf_Transmit
 *          - 将从 CanIf 收到的 N-PDU 重组为 I-PDU → PduR
 *          - FC 流控帧收发
 */

#ifndef CANTP_H
#define CANTP_H

#include "Std_Types.h"
#include "ComStack_Types.h"        /* PduInfoType */

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief CAN TP PDU ID */
typedef uint16_t CanTp_PduIdType;

/** @brief CAN TP 帧类型 (ISO 15765-2 PCI 字节高 4 位) */
typedef enum {
    CANTP_SF = 0,  /**< Single Frame        — 单帧，数据 ≤ 7 bytes */
    CANTP_FF = 1,  /**< First Frame         — 首帧，含 12-bit 总长度 */
    CANTP_CF = 2,  /**< Consecutive Frame   — 连续帧，含 4-bit 序号 */
    CANTP_FC = 3,  /**< Flow Control        — 流控帧，含 BS + STmin */
} CanTp_FrameType;

/** @brief CAN TP 内部状态 */
typedef enum {
    CANTP_IDLE = 0,       /**< 空闲 */
    CANTP_WAIT_FC = 1,    /**< 发送 FF 后等待 FC */
    CANTP_SENDING_CF = 2, /**< 正在发送 CF */
    CANTP_RECEIVING = 3,  /**< 正在接收多帧 */
} CanTp_StateType;

/* ===================================================================
 *  API 函数声明 (AUTOSAR 标准签名 — 直接使用 PduInfoType*)
 * =================================================================== */

/** @brief 初始化 CanTp 模块 */
void CanTp_Init(void);

/**
 * @brief 请求 CAN TP 发送 (SWS_CanTp_00020)
 * @param TxPduId      TP 通道 PDU ID
 * @param PduInfoPtr   待发送的 I-PDU 数据
 * @return             E_OK=成功, E_NOT_OK=失败
 */
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/**
 * @brief 接收指示 — CanIf 收到 N-PDU 后回调 (SWS_CanTp_00022)
 * @param RxPduId     接收通道 PDU ID
 * @param PduInfoPtr  收到的 N-PDU 数据 (8 字节 CAN 帧，含 PCI)
 */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/**
 * @brief 发送确认 — CanIf 发送完成后回调 (SWS_CanTp_00024)
 * @param TxPduId  已发送的 TP 通道 PDU ID
 */
void CanTp_TxConfirmation(PduIdType TxPduId);

/**
 * @brief CanTp 周期处理函数 (AUTOSAR MainFunction)
 *
 * @note  由 EcuM_MainFunction 周期调用，驱动:
 *        - TX 多帧 FC 流控状态机 (WAIT_FC → SENDING_CF → IDLE)
 *        - RX 重组超时检测 (N_Cr)
 *        - 定时器精度取决于 Log_GetTimeMs() (ARM DWT, 1ms)
 */
void CanTp_MainFunction(void);

/* ===================================================================
 *  PCI 编解码 (内部使用，但暴露给测试)
 * =================================================================== */

/** @brief 根据数据长度编码 SF (Single Frame) PCI 字节 */
uint8_t CanTp_EncodeSF(uint8_t dataLength);

/** @brief 编码 FF (First Frame) PCI 字节 */
void    CanTp_EncodeFF(uint16_t totalLength, uint8_t pci[2]);

/** @brief 编码 CF (Consecutive Frame) PCI 字节 */
uint8_t CanTp_EncodeCF(uint8_t seqNum);

/** @brief 编码 FC (Flow Control) PCI 字节 */
void    CanTp_EncodeFC(uint8_t bs, uint8_t stmin, uint8_t pci[3]);

/** @brief 解码 PCI 字节，返回帧类型 */
CanTp_FrameType CanTp_DecodePci(const uint8_t *data, uint8_t *sfLen,
                                uint16_t *ffTotalLen, uint8_t *cfSeqNum);

#endif /* CANTP_H */