/**
 * @file    Can.h
 * @brief   AUTOSAR CP MCAL Can 驱动 — CAN 2.0 通信接口
 *
 * @note    底层调用 NXP FlexCAN SDK 驱动，头文件不暴露任何 SDK 类型
 *          所有 API 遵循 AUTOSAR CAN Driver 规范命名
 */

#ifndef MCAL_CAN_H_
#define MCAL_CAN_H_

#include <stdint.h>
#include <stdbool.h>
#include "status.h"           /* status_t, STATUS_SUCCESS/ERROR/BUSY */

/* ===================================================================
 *  类型定义 (AUTOSAR MCAL 自有，不含 flexcan_driver.h)
 * =================================================================== */

/** @brief CAN 控制器 ID */
typedef enum {
    CAN_CONTROLLER_0   = 0U,
    CAN_CONTROLLER_MAX
} Can_ControllerType;

/** @brief CAN 控制器工作模式 */
typedef enum {
    CAN_CS_UNINIT  = 0U,
    CAN_CS_STARTED = 1U,
    CAN_CS_STOPPED = 2U
} Can_ControllerStateType;

/** @brief FlexCAN 运行模式 (MCAL 自有枚举，非 SDK) */
typedef enum {
    CAN_MODE_NORMAL    = 0U,
    CAN_MODE_FREEZE    = 1U,
    CAN_MODE_LOOPBACK  = 4U
} Can_ModeType;

/** @brief CAN Mailbox (Hardware Object) 配置 */
typedef struct {
    uint32_t id;
    bool     is_extended;
    bool     is_remote;
} Can_HardwareObject;

/** @brief CAN PDU 数据单元 */
typedef struct {
    uint32_t id;
    uint8_t  length;
    bool     is_extended;
    bool     is_remote;
    uint8_t  data[8];
} Can_PduType;

/* ===================================================================
 *  配置类型 — 仅含 MCAL 自有字段，上层无需知晓 FlexCAN SDK
 * =================================================================== */

typedef struct {
    /* --- 控制器 --- */
    uint8_t       controller;

    /* --- Mailbox 配置 --- */
    uint8_t       max_num_mb;
    uint8_t       num_id_filters;
    bool          is_rx_fifo_needed;
    Can_ModeType  flexcan_mode;

    /* --- 位时序 (映射到 SDK flexcan_time_segment_t) --- */
    uint8_t       prop_seg;
    uint8_t       phase_seg1;
    uint8_t       phase_seg2;
    uint8_t       pre_divider;
    uint8_t       r_jumpwidth;

    /* --- MB 分配 --- */
    uint8_t                     num_tx_mailboxes;
    uint8_t                     num_rx_mailboxes;
    const Can_HardwareObject   *tx_mailboxes;
    const Can_HardwareObject   *rx_mailboxes;
} Can_ConfigType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

status_t               Can_Init(const Can_ConfigType *ConfigPtr);
void                   Can_DeInit(void);
void                   Can_SetControllerMode(Can_ControllerType     Controller,
                                             Can_ControllerStateType Transition);
status_t               Can_Write(uint8_t Controller, uint8_t Hth,
                                 const Can_PduType *PduInfo);
status_t               Can_Read(uint8_t Controller, uint8_t Hrh,
                                Can_PduType *PduInfo);

#endif /* MCAL_CAN_H_ */