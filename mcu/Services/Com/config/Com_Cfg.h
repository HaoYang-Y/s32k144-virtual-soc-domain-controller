/**
 * @file    Com_Cfg.h
 * @brief   COM 模块配置 — 信号到 IPDU 映射
 */

#ifndef COM_CFG_H
#define COM_CFG_H

#include "Std_Types.h"

/** @brief 信号组数量 */
#define COM_SIGNAL_GROUP_COUNT      3U

/** @brief COM 信号配置 */
typedef struct {
    uint16_t signal_id;
    uint16_t ipdu_id;
    uint8_t  bit_position;
    uint8_t  bit_size;
    uint8_t  is_signed;
} Com_SignalConfigType;

/** @brief IPDU 配置 */
typedef struct {
    uint16_t ipdu_id;
    uint8_t  dlc;
    uint32_t can_id;
    uint16_t cycle_time_ms;          /* 0=事件触发 */
} Com_IPduConfigType;

extern const Com_SignalConfigType Com_SignalConfig[];
extern const Com_IPduConfigType   Com_IPduConfig[];

#endif /* COM_CFG_H */