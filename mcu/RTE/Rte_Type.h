/**
 * @file    Rte_Type.h
 * @brief   [SKELETON] RTE SWC 间数据类型定义
 *
 * @note    对应 ARXML 中 <SWC>→<DataTypes> 定义
 *          手动实现 AUTOSAR ImplementationDataType / ApplicationDataType
 */

#ifndef RTE_TYPE_H
#define RTE_TYPE_H

#include "Std_Types.h"

/** @brief 车辆速度 (uint16, 分辨率 0.01 km/h, 最大 655.35 km/h) */
typedef uint16_t Rte_VehicleSpeedType;

/** @brief 引擎转速 (uint16, 分辨率 0.125 rpm) */
typedef uint16_t Rte_EngineRpmType;

/** @brief 踏板位置 (uint8, 分辨率 0.4%) */
typedef uint8_t  Rte_AcceleratorPedalType;

/** @brief 制动位置 (uint8, 分辨率 0.4%) */
typedef uint8_t  Rte_BrakePositionType;

/** @brief 转向角 (int16, 分辨率 0.1 deg, 偏移 -780.0) */
typedef int16_t  Rte_SteeringAngleType;

#endif /* RTE_TYPE_H */
