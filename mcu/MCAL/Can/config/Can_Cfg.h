/**
 * @file    Can_Cfg.h
 * @brief   CAN 驱动预编译配置（AUTOSAR CP Pre-compile config）
 */

#ifndef CAN_CFG_H
#define CAN_CFG_H

#include "Std_Types.h"
#include "Can.h"

#define CAN_CONTROLLER_COUNT        1U
#define CAN_MAILBOX_COUNT           16U
#define CAN_DEV_ERROR_DETECT        STD_ON
#define CAN_WAKEUP_FEATURE          STD_OFF
#define CAN_MULTIPLE_TX_QUEUE       STD_OFF

#define CAN_INTERRUPT_ENABLE        STD_ON

#define CAN_RX_HARDWARE_OBJECTS     8U
#define CAN_TX_HARDWARE_OBJECTS     8U

extern const Can_ConfigType Can_Config;

#endif /* CAN_CFG_H */
