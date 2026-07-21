/**
 * @file    Can.h
 * @brief   AUTOSAR CP MCAL Can 驱动 — CAN 2.0 通信接口
 *
 * @note    对标 AUTOSAR SWS_Can 规范:
 *          - SWS_Can_00008:  Can_IdType
 *          - SWS_Can_00009:  Can_HwHandleType
 *          - SWS_Can_00018~21: Can_PduType (L-PDU)
 *          - SWS_Can_00098:  Can_SetControllerMode → Std_ReturnType
 *          - SWS_Can_00106:  Can_Write(Hth, PduInfo) — 无 Controller 参数
 *          - SWS_Can_00167:  Can_GetControllerErrorState
 *          - SWS_Can_00130:  Can_GetControllerMode
 *
 *          底层调用 NXP FlexCAN SDK 驱动，头文件不暴露任何 SDK 类型。
 */

#ifndef MCAL_CAN_H_
#define MCAL_CAN_H_

#include <stdint.h>
#include <stdbool.h>
#include "status.h"           /* status_t, STATUS_SUCCESS/ERROR/BUSY */
#include "Std_Types.h"        /* Std_ReturnType, E_OK, E_NOT_OK */

/* ===================================================================
 *  类型定义 (AUTOSAR SWS_Can)
 * =================================================================== */

/** @brief CAN 消息 ID 类型 (SWS_Can_00008) */
typedef uint32_t Can_IdType;

/** @brief CAN Hardware Object Handle (SWS_Can_00009)
 *  用于 Can_Write 指定 TX Mailbox，或 Can_Read 指定 RX Mailbox */
typedef uint16_t Can_HwHandleType;

/** @brief CAN 控制器 ID */
typedef enum {
    CAN_CONTROLLER_0   = 0U,
    CAN_CONTROLLER_MAX
} Can_ControllerType;

/** @brief CAN 控制器工作模式 (SWS_Can_00015) */
typedef enum {
    CAN_CS_UNINIT  = 0U,
    CAN_CS_STARTED = 1U,
    CAN_CS_STOPPED = 2U
} Can_ControllerStateType;

/** @brief CAN 控制器错误状态 (SWS_Can_00016) */
typedef enum {
    CAN_ERRORSTATE_ACTIVE  = 0U,   /**< Error Active  — 正常通信 */
    CAN_ERRORSTATE_PASSIVE = 1U,   /**< Error Passive — 可通信但受限 */
    CAN_ERRORSTATE_BUSOFF  = 2U,   /**< Bus Off       — 脱离总线 */
} Can_ErrorStateType;

/** @brief FlexCAN 运行模式 (MCAL 自有枚举，非 SDK) */
typedef enum {
    CAN_MODE_NORMAL    = 0U,
    CAN_MODE_FREEZE    = 1U,
    CAN_MODE_LOOPBACK  = 4U
} Can_ModeType;

/** @brief CAN Mailbox (Hardware Object) 配置 */
typedef struct {
    Can_IdType   id;
    bool         is_extended;
    bool         is_remote;
} Can_HardwareObject;

/** @brief CAN L-PDU 数据单元 (SWS_Can_00018~00021)
 *
 *  CAN 总线帧的 MCAL 层抽象 — 固定 8 字节 data 数组。
 *  CanIf 将 PduInfoType(N-PDU) 转换为 Can_PduType(L-PDU) 后调用 Can_Write。
 *  接收时 Can_Read 将硬件帧填充为 Can_PduType 后交给 CanIf。
 */
typedef struct {
    Can_IdType   id;            /**< CAN 报文 ID (SWS_Can_00018) */
    uint8_t      length;        /**< 数据长度 DLC 0-8 (SWS_Can_00019) */
    bool         is_extended;   /**< 扩展帧标志 (项目扩展) */
    bool         is_remote;     /**< 远程帧标志 (项目扩展) */
    uint8_t      data[8];       /**< CAN 帧数据 (SWS_Can_00021) */
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

Std_ReturnType Can_Init(const Can_ConfigType *ConfigPtr);

Std_ReturnType Can_DeInit(void);

/** @brief 设置控制器模式 (SWS_Can_00098) */
Std_ReturnType Can_SetControllerMode(Can_ControllerType     Controller,
                                     Can_ControllerStateType Transition);

/** @brief 发送 CAN 报文 — 只传 HTH，Controller 由 HTH 编码 (SWS_Can_00106) */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

/** @brief 轮询接收 CAN 报文 (项目扩展 — AUTOSAR 标准 RX 为中断回调) */
status_t       Can_Read(uint8_t Controller, uint8_t Hrh,
                        Can_PduType *PduInfo);

/** @brief 获取控制器错误状态 (SWS_Can_00167) */
Std_ReturnType Can_GetControllerErrorState(Can_ControllerType  Controller,
                                           Can_ErrorStateType *ErrorStatePtr);

/** @brief 获取控制器当前模式 (SWS_Can_00130) */
Std_ReturnType Can_GetControllerMode(Can_ControllerType      Controller,
                                     Can_ControllerStateType *ModePtr);

#endif /* MCAL_CAN_H_ */