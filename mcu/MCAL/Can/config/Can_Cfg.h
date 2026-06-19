/**
 * @file    Can_Cfg.h
 * @brief   CAN 驱动预编译配置（AUTOSAR CP Pre-compile config）
 *
 * @note    对应 AUTOSAR CP Can_Cfg.h 规范
 *          在编译期确定 CAN 控制器数量、邮箱总数、中断策略等
 *          当前配置：1 控制器, 16 MB, 中断驱动
 */

#ifndef CAN_CFG_H
#define CAN_CFG_H

#include "Std_Types.h"

/* ===================================================================
 *  控制器配置
 * =================================================================== */

/** @brief CAN 控制器数 */
#define CAN_CONTROLLER_COUNT        1U
/** @brief 每个控制器的邮箱总数 */
#define CAN_MAILBOX_COUNT           16U
/** @brief CAN 驱动为开发错误检测版本（相对于生产版本） */
#define CAN_DEV_ERROR_DETECT        STD_ON
/** @brief 是否启用 CAN 内部 Loopback */
#define CAN_WAKEUP_FEATURE          STD_OFF
/** @brief 是否支持多 PDUTx 请求同时处理 */
#define CAN_MULTIPLE_TX_QUEUE       STD_OFF

/* ===================================================================
 *  中断配置
 * =================================================================== */
/** @brief 使用中断方式收发 */
#define CAN_INTERRUPT_ENABLE        STD_ON

/* ===================================================================
 *  硬件过滤配置
 * =================================================================== */
#define CAN_RX_HARDWARE_OBJECTS     8U
#define CAN_TX_HARDWARE_OBJECTS     8U

/* ===================================================================
 *  CAN 信号映射表 (DBC 等价)
 * =================================================================== */

/** @brief CAN 信号定义 (一个 CAN ID 帧内的一个信号) */
typedef struct {
    uint32_t can_id;
    uint8_t  start_bit;
    uint8_t  length;
    uint8_t  is_big_endian;   /* 0=Intel(LSB), 1=Motorola(MSB) */
    uint16_t scale_num;       /* 物理值 = raw * scale_num / scale_den + offset */
    uint16_t scale_den;
    int16_t  offset;
    uint8_t  target_channel;  /* SPI 输出通道 */
} Can_SignalDefType;

extern const Can_SignalDefType Can_SignalMap[];
extern const uint8_t           Can_SignalMap_Count;

#endif /* CAN_CFG_H */
