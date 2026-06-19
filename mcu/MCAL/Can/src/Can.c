/**
 * @file    Can.c
 * @brief   AUTOSAR CP MCAL Can 驱动实现 — 封装 NXP FlexCAN SDK
 *
 * @note    所有 FlexCAN SDK 依赖 (flexcan_driver.h, flexcan_hw_access.h...)
 *          仅在本 .c 文件内可见。对外暴露纯 AUTOSAR 类型。
 */

#include "Can.h"
#include <stddef.h>

/* ===== SDK 依赖全部下沉到此处 ===== */
#include "flexcan_driver.h"
#include "flexcan_hw_access.h"

/* ===================================================================
 *  模块级私有变量
 * =================================================================== */

static Can_ConfigType  Can_Config;          /* 配置副本 */
static flexcan_state_t Can_SdkState;        /* SDK 内部状态 (仅 .c 可见) */
static bool            Can_Initialized = false;
static Can_ControllerStateType Can_State[CAN_CONTROLLER_MAX] = { CAN_CS_UNINIT };

/* ===================================================================
 *  内部: MCAL 配置 → SDK 配置 转换
 * =================================================================== */

static void Can_BuildSdkConfig(const Can_ConfigType *mcal,
                               flexcan_user_config_t *sdk)
{
    sdk->max_num_mb        = mcal->max_num_mb;
    sdk->num_id_filters    = mcal->num_id_filters;
    sdk->is_rx_fifo_needed = mcal->is_rx_fifo_needed;

    /* MCAL Can_ModeType → SDK flexcan_modes_t (值一致) */
    sdk->flexcanMode       = (flexcan_operation_modes_t)mcal->flexcan_mode;

    /* 未使用 RxFIFO 时 transfer_type 无效，填中断模式即可 */
    sdk->transfer_type     = FLEXCAN_RXFIFO_USING_INTERRUPTS;

    /* 位时序 */
    sdk->bitrate.propSeg    = mcal->prop_seg;
    sdk->bitrate.phaseSeg1  = mcal->phase_seg1;
    sdk->bitrate.phaseSeg2  = mcal->phase_seg2;
    sdk->bitrate.preDivider = mcal->pre_divider;
    sdk->bitrate.rJumpwidth = mcal->r_jumpwidth;
}

/* ===================================================================
 *  Can_ConvertPduToDataInfo (内部)
 * =================================================================== */

static void Can_ConvertPduToDataInfo(const Can_PduType *pdu,
                                     flexcan_data_info_t *tx_info)
{
    if ((pdu == NULL) || (tx_info == NULL)) return;

    tx_info->msg_id_type = pdu->is_extended ? FLEXCAN_MSG_ID_EXT
                                            : FLEXCAN_MSG_ID_STD;
    tx_info->data_length = pdu->length;
    tx_info->is_remote   = pdu->is_remote;
#if FEATURE_CAN_HAS_FD
    tx_info->fd_enable  = false;
    tx_info->fd_padding = 0U;
    tx_info->enable_brs = false;
#endif
}

/* ===================================================================
 *  Can_ConvertMbToPdu (内部)
 * =================================================================== */

static void Can_ConvertMbToPdu(const flexcan_msgbuff_t *mb,
                               Can_PduType *pdu)
{
    uint8_t i;

    if ((mb == NULL) || (pdu == NULL)) return;

    pdu->id          = mb->msgId;
    pdu->length      = mb->dataLen;
    pdu->is_extended = ((mb->cs & CAN_WMBn_CS_IDE_MASK) != 0U);
    pdu->is_remote   = ((mb->cs & CAN_WMBn_CS_RTR_MASK) != 0U);

    for (i = 0U; i < 8U; i++) {
        pdu->data[i] = (i < mb->dataLen) ? mb->data[i] : 0U;
    }
}

/* ===================================================================
 *  Can_Init
 * =================================================================== */

status_t Can_Init(const Can_ConfigType *ConfigPtr)
{
    flexcan_user_config_t sdk_cfg;

    if (ConfigPtr == NULL) return STATUS_ERROR;

    /* 保存 MCAL 配置副本 */
    Can_Config = *ConfigPtr;

    /* MCAL → SDK 转换 + 初始化 */
    Can_BuildSdkConfig(ConfigPtr, &sdk_cfg);

    if (FLEXCAN_DRV_Init(Can_Config.controller,
                          &Can_SdkState, &sdk_cfg) != STATUS_SUCCESS)
        return STATUS_ERROR;

    Can_Initialized = true;
    Can_State[Can_Config.controller] = CAN_CS_STOPPED;
    return STATUS_SUCCESS;
}

/* ===================================================================
 *  Can_DeInit
 * =================================================================== */

void Can_DeInit(void)
{
    if (!Can_Initialized) return;
    (void)FLEXCAN_DRV_Deinit(Can_Config.controller);
    Can_Initialized = false;
    Can_State[Can_Config.controller] = CAN_CS_UNINIT;
}

/* ===================================================================
 *  Can_SetControllerMode
 * =================================================================== */

void Can_SetControllerMode(Can_ControllerType     Controller,
                           Can_ControllerStateType Transition)
{
    uint8_t i;
    flexcan_data_info_t mb_info;

    if (Controller >= CAN_CONTROLLER_MAX || !Can_Initialized) return;

    if (Transition == CAN_CS_STARTED) {
        uint8_t tx_count = Can_Config.num_tx_mailboxes;
        uint8_t rx_count = Can_Config.num_rx_mailboxes;

        /* 配置 TX MB */
        for (i = 0U; i < tx_count; i++) {
            const Can_HardwareObject *hw = &Can_Config.tx_mailboxes[i];
            mb_info.msg_id_type = hw->is_extended ? FLEXCAN_MSG_ID_EXT
                                                  : FLEXCAN_MSG_ID_STD;
            mb_info.data_length = 8U;
            mb_info.is_remote   = hw->is_remote;
#if FEATURE_CAN_HAS_FD
            mb_info.fd_enable  = false;
            mb_info.fd_padding = 0U;
            mb_info.enable_brs = false;
#endif
            (void)FLEXCAN_DRV_ConfigTxMb(Can_Config.controller,
                                         i, &mb_info, hw->id);
        }

        /* 配置 RX MB */
        for (i = 0U; i < rx_count; i++) {
            const Can_HardwareObject *hw = &Can_Config.rx_mailboxes[i];
            uint8_t mb_idx = tx_count + i;
            mb_info.msg_id_type = hw->is_extended ? FLEXCAN_MSG_ID_EXT
                                                  : FLEXCAN_MSG_ID_STD;
            mb_info.data_length = 8U;
            mb_info.is_remote   = hw->is_remote;
#if FEATURE_CAN_HAS_FD
            mb_info.fd_enable  = false;
            mb_info.fd_padding = 0U;
            mb_info.enable_brs = false;
#endif
            (void)FLEXCAN_DRV_ConfigRxMb(Can_Config.controller,
                                         mb_idx, &mb_info, hw->id);
        }

        Can_State[Controller] = CAN_CS_STARTED;
    } else if (Transition == CAN_CS_STOPPED) {
        Can_State[Controller] = CAN_CS_STOPPED;
    }
}

/* ===================================================================
 *  Can_Write
 * =================================================================== */

status_t Can_Write(uint8_t Controller, uint8_t Hth,
                   const Can_PduType *PduInfo)
{
    flexcan_data_info_t tx_info;

    if (!Can_Initialized)                        return STATUS_ERROR;
    if (Controller != Can_Config.controller)     return STATUS_ERROR;
    if (Can_State[Controller] != CAN_CS_STARTED) return STATUS_ERROR;
    if (Hth >= Can_Config.num_tx_mailboxes)      return STATUS_ERROR;
    if (PduInfo == NULL)                         return STATUS_ERROR;

    Can_ConvertPduToDataInfo(PduInfo, &tx_info);

    return FLEXCAN_DRV_Send(Can_Config.controller,
                            Hth, &tx_info, PduInfo->id, PduInfo->data);
}

/* ===================================================================
 *  Can_Read
 * =================================================================== */

status_t Can_Read(uint8_t Controller, uint8_t Hrh,
                  Can_PduType *PduInfo)
{
    flexcan_msgbuff_t mb;
    status_t          ret;

    if (!Can_Initialized)                        return STATUS_ERROR;
    if (Controller != Can_Config.controller)     return STATUS_ERROR;
    if (Can_State[Controller] != CAN_CS_STARTED) return STATUS_ERROR;
    if (Hrh < Can_Config.num_tx_mailboxes)       return STATUS_ERROR;
    if (PduInfo == NULL)                         return STATUS_ERROR;

    ret = FLEXCAN_DRV_Receive(Can_Config.controller, Hrh, &mb);
    if (ret == STATUS_SUCCESS) {
        Can_ConvertMbToPdu(&mb, PduInfo);
    }
    return ret;
}