/**
 * @file    ComStack_Types.h
 * @brief   AUTOSAR CP 通信栈共享类型定义
 *
 * @note    对标 AUTOSAR SWS_ComStackTypes:
 *          - SWS_ComStackTypes_00031: PduIdType
 *          - SWS_ComStackTypes_00032: PduLengthType
 *          - SWS_ComStackTypes_00033: PduInfoType
 *
 *          PduInfoType 是 AUTOSAR COM Stack 中唯一的 PDU 数据交换类型。
 *          CanTp/CanIf/PduR/Com 所有模块的 API 都使用 PduInfoType* 传递。
 *          同一个 PduInfoType 在不同层级有不同的角色名：
 *            Com ↔ PduR:       I-PDU
 *            CanTp ↔ CanIf:    N-PDU
 */

#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"

/** @brief PDU ID 类型 (SWS_ComStackTypes_00031) */
typedef uint16_t PduIdType;

/** @brief PDU 长度类型 (SWS_ComStackTypes_00032) */
typedef uint16_t PduLengthType;

/** @brief 统一的 PDU 信息结构 (SWS_ComStackTypes_00033) */
typedef struct {
    PduIdType     SduId;        /**< PDU ID */
    PduLengthType SduLength;    /**< PDU 数据长度 (bytes) */
    uint8_t      *SduDataPtr;   /**< 指向 PDU 数据的指针 */
} PduInfoType;

#endif /* COMSTACK_TYPES_H */