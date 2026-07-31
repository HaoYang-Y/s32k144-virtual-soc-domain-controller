/**
 * @file    Can.h
 * @brief   AUTOSAR CP MCAL Can 驱动 — CAN 2.0 多路通信接口
 *
 * @note    对标 AUTOSAR SWS_Can 规范:
 *          - SWS_Can_00009:  Can_HwHandleType — HTH 编码 Controller + MB Index
 *          - SWS_Can_00098:  Can_SetControllerMode → Std_ReturnType
 *          - SWS_Can_00106:  Can_Write(Hth, PduInfo) — 无 Controller 参数
 *          - SWS_Can_00167:  Can_GetControllerErrorState
 *          - SWS_Can_00130:  Can_GetControllerMode
 *
 *          HTH 编码: bit[15:8]=Controller  bit[7:0]=MB Index
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
 *  bit[15:8]=Controller  bit[7:0]=MB Index
 *  Can_Write 只传 HTH，Controller 由 HTH 高位解码 */
typedef uint16_t Can_HwHandleType;

/** @brief HTH 编解码宏 */
#define CAN_HTH_MAKE(ctrl, mb)    ((Can_HwHandleType)(((uint8_t)(ctrl) << 8U) | ((uint8_t)(mb) & 0xFFU)))
#define CAN_HTH_CTRL(hth)         ((uint8_t)((hth) >> 8U))
#define CAN_HTH_MB(hth)           ((uint8_t)((hth) & 0xFFU))

/** @brief CAN 控制器 ID */
typedef enum {
    CAN_CONTROLLER_0   = 0U,
    CAN_CONTROLLER_1   = 1U,
    CAN_CONTROLLER_2   = 2U,
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
    CAN_ERRORSTATE_ACTIVE  = 0U,
    CAN_ERRORSTATE_PASSIVE = 1U,
    CAN_ERRORSTATE_BUSOFF  = 2U,
} Can_ErrorStateType;

/** @brief FlexCAN 运行模式 */
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

/** @brief CAN L-PDU 数据单元 (SWS_Can_00018~00021) */
typedef struct {
    Can_IdType   id;
    uint8_t      length;
    bool         is_extended;
    bool         is_remote;
    uint8_t      data[8];
} Can_PduType;

/* ===================================================================
 *  配置类型
 * =================================================================== */

typedef struct {
    uint8_t       max_num_mb;
    uint8_t       num_id_filters;
    bool          is_rx_fifo_needed;
    Can_ModeType  flexcan_mode;

    uint8_t       prop_seg;
    uint8_t       phase_seg1;
    uint8_t       phase_seg2;
    uint8_t       pre_divider;
    uint8_t       r_jumpwidth;

    uint8_t                     num_tx_mailboxes;
    uint8_t                     num_rx_mailboxes;
    const Can_HardwareObject   *tx_mailboxes;
    const Can_HardwareObject   *rx_mailboxes;
} Can_ConfigType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/** @brief 初始化指定 CAN 控制器 (SWS_Can_00013)
 *  @param Controller  控制器 ID (CAN_CONTROLLER_0~2)
 *  @param ConfigPtr   指向控制器配置结构，可为 NULL（只初始化软件状态） */
Std_ReturnType Can_Init(Can_ControllerType Controller, const Can_ConfigType *ConfigPtr);

Std_ReturnType Can_DeInit(void);

Std_ReturnType Can_SetControllerMode(Can_ControllerType     Controller,
                                     Can_ControllerStateType Transition);

/** @brief 发送 CAN 报文 — HTH 编码 Controller+MB (SWS_Can_00106) */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

/** @brief 轮询接收 — 中断模式下由 Can_MainFunctionRx 内部使用 */
status_t       Can_Read(uint8_t Controller, uint8_t Hrh,
                        Can_PduType *PduInfo);

Std_ReturnType Can_GetControllerErrorState(Can_ControllerType  Controller,
                                           Can_ErrorStateType *ErrorStatePtr);
Std_ReturnType Can_GetControllerMode(Can_ControllerType      Controller,
                                     Can_ControllerStateType *ModePtr);

/** @brief 使能所有已初始化控制器的 RX 中断 */
void Can_EnableInterrupts(void);

/** @brief RX 通知回调 — 上层模块注册，Can.c 在收到帧时调用 */
typedef void (*Can_RxNotificationType)(Can_ControllerType Controller,
                                       uint8_t Hrh,
                                       const Can_PduType *PduInfo,
                                       const uint8_t  *data);

void Can_RegisterRxCallback(Can_RxNotificationType callback);

/** @brief TX 完成通知回调 — 上层模块注册，Can.c 在发送完成后调用
 *  @param Controller  完成发送的控制器 ID
 *  @param MbIndex     完成发送的 TX Mailbox 索引 (与 HTH 低字节一致) */
typedef void (*Can_TxConfirmationType)(Can_ControllerType Controller,
                                       uint8_t MbIndex);

void Can_RegisterTxCallback(Can_TxConfirmationType callback);

/** @brief 周期调用: 所有控制器的 ISR 标记 → 消费 → 回调 */
bool Can_MainFunctionRx(void);

/** @brief 周期调用: 所有控制器的 TX 完成确认 (SWS_Can_00047) */
void Can_MainFunctionWrite(void);

#endif /* MCAL_CAN_H_ */
