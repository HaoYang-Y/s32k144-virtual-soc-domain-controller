/**
 * @file    CanTp_Cfg.h
 * @brief   CAN Transport Layer 配置 — 多帧收发参数
 */

#ifndef CANTP_CFG_H
#define CANTP_CFG_H

#include "Std_Types.h"

/** @brief CAN TP 超时参数 (ISO 15765-2) */
#define CANTP_AS_TIMEOUT_MS        1000U   /* N_As: 发送确认 */
#define CANTP_AR_TIMEOUT_MS        1000U   /* N_Ar: 接收确认 */
#define CANTP_BS_TIMEOUT_MS        1000U   /* N_Bs: 接收方发送流控 */
#define CANTP_CR_TIMEOUT_MS        1000U   /* N_Cr: 连续帧到达 */

/** @brief 流控参数 */
#define CANTP_BLOCK_SIZE           8U      /* STmin 前的连续帧数 */
#define CANTP_STMIN_US             100U    /* 连续帧最小间隔 */

#endif /* CANTP_CFG_H */