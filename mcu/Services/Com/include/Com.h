/**
 * @file    Com.h
 * @brief   [SKELETON] Communication 模块 — AUTOSAR CP Services 层
 *
 * @note    信号级通信：将 SWC 信号打包/解包为 I-PDU
 *          Com_SendSignal / Com_ReceiveSignal
 */

#ifndef COM_H
#define COM_H

#include "Std_Types.h"

/* ===================================================================
 *  类型定义
 * =================================================================== */

typedef uint16_t Com_SignalIdType;
typedef uint16_t Com_IPduIdType;

/** @brief 信号值结构 */
typedef struct {
    uint8_t  *data;
    uint16_t  length;
} Com_SignalType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

void Com_Init(void);
void Com_MainFunction(void);

void   Com_SendSignal(Com_SignalIdType SignalId, const void *SignalData);
void   Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalData);
uint8_t Com_SendSignalGroup(Com_SignalIdType GroupId);
uint8_t Com_ReceiveSignalGroup(Com_SignalIdType GroupId);

void Com_IPduGroupStart(Com_IPduIdType GroupId);
void Com_IPduGroupStop(Com_IPduIdType GroupId);

#endif /* COM_H */
