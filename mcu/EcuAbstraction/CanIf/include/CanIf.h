/**
 * @file    CanIf.h
 * @brief   [SKELETON] CAN Interface — AUTOSAR CP ECU Abstraction 层
 *
 * @note    作用：统一封装 MCAL Can 的收发操作，向上层 (PduR/Com) 提供
 *          与硬件无关的 CAN 通信接口。
 *          API: CanIf_Transmit, CanIf_RxIndication, CanIf_TxConfirmation
 */

#ifndef CANIF_H
#define CANIF_H

#include "Std_Types.h"

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief CAN 控制器 ID (对应 AUTOSAR CanIf_ControllerType) */
typedef uint8_t CanIf_ControllerType;

/** @brief CAN PDU ID (对应 AUTOSAR CanIf_PduType) */
typedef uint16_t CanIf_PduIdType;

/** @brief CAN Interface PDU 结构 */
typedef struct {
    CanIf_PduIdType  id;
    uint8_t          length;
    uint8_t          *data;
} CanIf_PduType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 向上层通知接收到的 CAN 消息
 * @param Controller  控制器 ID
 * @param PduPtr      指向 PDU 数据的指针
 */
void CanIf_RxIndication(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr);

/**
 * @brief 向上层通知发送完成
 * @param Controller  控制器 ID
 * @param PduPtr      指向已发送 PDU 的指针
 */
void CanIf_TxConfirmation(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr);

/**
 * @brief 请求 CAN 发送 (由 PduR 或上层调用)
 * @param Controller  控制器 ID
 * @param PduPtr      指向待发送 PDU 的指针
 * @return            0=成功, 1=忙, 其他=错误
 */
uint8_t CanIf_Transmit(CanIf_ControllerType Controller, CanIf_PduType *PduPtr);

/**
 * @brief 初始化 CanIf 模块
 */
void CanIf_Init(void);

#endif /* CANIF_H */
