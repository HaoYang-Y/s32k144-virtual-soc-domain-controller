/**
 * @file    Swc_SignalGateway_Cfg.h
 * @brief   信号网关 SWC 配置 — 应用参数
 *
 * @note    编译期应用配置，对应 AUTOSAR SWC implementation configuration
 */

#ifndef SWC_SIGNALGATEWAY_CFG_H
#define SWC_SIGNALGATEWAY_CFG_H

#include "Std_Types.h"

/** @brief 主循环周期 (ms) */
#define SWC_MAIN_CYCLE_MS          10U

/** @brief CAN 消息发送周期 (ms) */
#define SWC_CAN_TX_CYCLE_MS        500U

/** @brief SPI 状态上报周期 (ms) */
#define SWC_SPI_REPORT_CYCLE_MS    100U

#endif /* SWC_SIGNALGATEWAY_CFG_H */