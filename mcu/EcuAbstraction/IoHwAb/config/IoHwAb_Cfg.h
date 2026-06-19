/**
 * @file    IoHwAb_Cfg.h
 * @brief   I/O 硬件抽象配置 — 信号 ID 到物理通道映射
 */

#ifndef IOHWAB_CFG_H
#define IOHWAB_CFG_H

#include "Std_Types.h"

/** @brief 抽象信号 ID 枚举 */
typedef enum {
    IOHWAB_SIGNAL_ACCELERATOR  = 0,   /* 加速踏板 (ADC0) */
    IOHWAB_SIGNAL_BRAKE        = 1,   /* 制动踏板 (ADC0) */
    IOHWAB_SIGNAL_DOOR         = 2,   /* 车门状态 (GPIO 输入) */
    IOHWAB_SIGNAL_LED          = 3,   /* LED (GPIO 输出) */
    IOHWAB_SIGNAL_COUNT
} IoHwAb_SignalId;

#endif /* IOHWAB_CFG_H */