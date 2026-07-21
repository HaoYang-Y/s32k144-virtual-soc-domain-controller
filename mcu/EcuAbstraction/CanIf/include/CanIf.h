/**
 * @file    CanIf.h
 * @brief   [AUTOSAR CP] CAN Interface — ECU Abstraction 层
 *
 * @note    对标 AUTOSAR SWS_CanIf:
 *          - SWS_CanIf_00050 / 00221: CanIf_Transmit(PduId, PduInfo*)
 *          - SWS_CanIf_00030 / 00204: CanIf_RxIndication(PduId, PduInfo*)
 *          - SWS_CanIf_00040 / 00211: CanIf_TxConfirmation(PduId)
 *
 *          AUTOSAR CanIf 不定义自己的 PDU 数据包装类型，
 *          而是直接使用 ComStack_Types.h 中的 PduInfoType*。
 *          PduId → CAN ID + Controller 的映射由 CanIf_PduConfig 表完成。
 */

#ifndef CANIF_H
#define CANIF_H

#include "Std_Types.h"
#include "ComStack_Types.h"        /* PduInfoType */

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief CAN 控制器 ID (AUTOSAR CanIf_ControllerType) */
typedef uint8_t CanIf_ControllerType;

/** @brief CAN PDU ID (AUTOSAR CanIf_PduIdType) */
typedef uint16_t CanIf_PduIdType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 初始化 CanIf 模块
 */
void CanIf_Init(void);

/**
 * @brief 请求 CAN 发送 (SWS_CanIf_00221)
 * @param TxPduId     PDU ID (用于查表获取 CAN ID + Controller)
 * @param PduInfoPtr  指向 PDU 信息的指针 (SduId, SduLength, SduDataPtr)
 * @return            E_OK=成功, E_NOT_OK=失败
 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);

/**
 * @brief 向上层通知接收到的 CAN 消息 (SWS_CanIf_00204)
 * @param RxPduId     接收到的 PDU ID
 * @param PduInfoPtr  指向接收 PDU 信息的指针
 */
void CanIf_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/**
 * @brief 向上层通知发送完成 (SWS_CanIf_00211)
 * @param TxPduId  已发送的 PDU ID
 */
void CanIf_TxConfirmation(PduIdType TxPduId);

/**
 * @brief 按 CAN ID 查找 PDU ID（RX 路径：CAN 帧 ID → PDU ID）
 * @param CanId  CAN 报文 ID
 * @return      匹配的 PDU ID，未找到返回 CANIF_PDU_COUNT
 */
CanIf_PduIdType CanIf_FindPduIdByCanId(uint32_t CanId);

#endif /* CANIF_H */