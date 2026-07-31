/**
 * @file    CanIf_Cfg.h
 * @brief   CAN Interface 配置 — CAN 控制器到 PDU 的映射
 */

#ifndef CANIF_CFG_H
#define CANIF_CFG_H

#include "Std_Types.h"

/** @brief 已配置的 CAN 控制器数 */
#define CANIF_CONTROLLER_COUNT  1U

/** @brief 开发错误检测开关 (STD_ON=启用 LOG_E 报告, STD_OFF=关闭) */
#define CANIF_DEV_ERROR_DETECT  STD_ON

/** @brief CAN Interface PDU 配置 */
typedef struct {
    uint16_t  pdu_id;
    uint8_t   controller_id;
    uint32_t  can_id;
    uint8_t   dlc;
    uint16_t  hth;   /* TX: 硬件发送句柄 (Controller+MB 编码); RX: 填 0 无意义 */
} CanIf_PduConfigType;

extern const CanIf_PduConfigType CanIf_PduConfig[];
extern const uint8_t             CanIf_PduConfig_Count;

#endif /* CANIF_CFG_H */