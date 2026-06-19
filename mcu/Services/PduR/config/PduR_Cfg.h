/**
 * @file    PduR_Cfg.h
 * @brief   PDU Router 配置 — PDU 路由表
 */

#ifndef PDUR_CFG_H
#define PDUR_CFG_H

#include "Std_Types.h"

/** @brief PDU 路由条目 */
typedef struct {
    uint16_t src_pdu_id;    /**< 源 PDU ID */
    uint16_t dst_pdu_id;    /**< 目标 PDU ID */
    uint8_t  src_module;    /**< 源模块: 0=Com, 1=CanIf, 2=SpiIf */
    uint8_t  dst_module;    /**< 目标模块 */
} PduR_RouteConfigType;

#define PDUR_ROUTE_COUNT        4U

extern const PduR_RouteConfigType PduR_RouteConfig[];

#endif /* PDUR_CFG_H */