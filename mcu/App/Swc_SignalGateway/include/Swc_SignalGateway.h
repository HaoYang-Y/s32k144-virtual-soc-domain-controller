/**
 * @file    Swc_SignalGateway.h
 * @brief   [SKELETON] 信号网关 SWC — 应用层软件组件
 *
 * @note    对应 AUTOSAR CP Application SWC 概念
 *          负责：采集 GPIO/ADC 信号 → 打包为 CAN 消息发送
 *          SPI 接收 SOC 指令 → 切换工作模式等
 */

#ifndef SWC_SIGNALGATEWAY_H
#define SWC_SIGNALGATEWAY_H

#include "Std_Types.h"

/* ===================================================================
 *  SWC 对外接口函数
 * =================================================================== */

/**
 * @brief 初始化信号网关 SWC
 */
void Swc_SignalGateway_Init(void);

/**
 * @brief SWC 主循环函数（周期调用）
 */
void Swc_SignalGateway_MainFunction(void);

/* ===================================================================
 *  CAN 信号发送接口（由 RTE 调用）
 * =================================================================== */

/**
 * @brief 发送车辆速度信号 (0x100 VehicleSpeed)
 * @param speed  速度值 (单位: km/h, 分辨率 0.01)
 */
void Rte_Write_VehicleSpeed(uint16 speed);

/**
 * @brief 发送引擎转速信号 (0x100 EngineRPM)
 * @param rpm 转速值 (单位: rpm, 分辨率 0.125)
 */
void Rte_Write_EngineRPM(uint16 rpm);

/**
 * @brief 发送踏板位置信号 (0x100 AcceleratorPedal)
 * @param pos 踏板位置百分比 (0-100, 分辨率 0.4%)
 */
void Rte_Write_AcceleratorPedal(uint8 pos);

#endif /* SWC_SIGNALGATEWAY_H */
