/**
 * @file    Rte.h
 * @brief   [SKELETON] Runtime Environment — AUTOSAR CP RTE
 *
 * @note    SWC 与 BSW 之间的运行时胶水代码
 *          手动实现的信号→I-PDU 映射和 SWC 间通信
 *
 *          RTE 负责:
 *          - SWC → BSW 的接口桥接
 *          - 信号级数据路由
 *          - 运行 SWC 主函数
 */

#ifndef RTE_H
#define RTE_H

#include "Std_Types.h"
#include "Rte_Type.h"

/* ===================================================================
 *  RTE 初始化
 * =================================================================== */

void Rte_Init(void);
void Rte_Start(void);
void Rte_MainFunction(void);

/* ===================================================================
 *  SWC 数据访问接口 (Rte_Read / Rte_Write)
 * =================================================================== */

void Rte_Write_VehicleSignal(uint8_t SignalId, const uint8_t *Data, uint8_t Length);
void Rte_Read_VehicleSignal(uint8_t SignalId, uint8_t *Data, uint8_t *Length);

#endif /* RTE_H */
