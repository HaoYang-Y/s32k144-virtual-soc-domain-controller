/**
 * @file    Can.c
 * @brief   AUTOSAR CP MCAL Can 驱动 — 基于 SDK CAN PAL 层实现
 *
 * @note    对外暴露 AUTOSAR Can_Init/Can_Write/Can_Read 标准接口，
 *          内部使用 NXP CAN PAL (can_pal.h) 简化状态管理和中断处理。
 *          关键: 每次 Can_Write 前必须重配 TX MB (CAN_ConfigTxBuff),
 *          否则 FLEXCAN_DRV_Send 的 MB 状态检查会返回 BUSY。
 *
 *          Can_Write(Hth, PduInfo) — AUTOSAR 标准签名，不传 Controller。
 *          Controller 从 Can_Config.controller 获取（内部维护）。
 */

#include "Can.h"
#include "can_pal.h"
#include "can_pal_mapping.h"
#include "can_pal_cfg.h"
#include <stddef.h>

/* ===================================================================
 *  宏: status_t ↔ Std_ReturnType 映射
 * =================================================================== */

#define CAN_STATUS_TO_STD_RET(s)  ((s) == STATUS_SUCCESS ? E_OK : E_NOT_OK)

/* ===================================================================
 *  模块级私有变量
 * =================================================================== */

static const can_instance_t Can_Instance = {
    .instType = CAN_INST_TYPE_FLEXCAN,
    .instIdx  = 0U  /* FlexCAN0 */
};

static can_user_config_t   Can_PalConfig;       /* PAL 配置副本 */
static can_buff_config_t   Can_TxBuffCfg;       /* TX MB 配置 */
static can_buff_config_t   Can_RxBuffCfg;       /* RX MB 配置 */
static Can_ConfigType      Can_Config;          /* MCAL 配置副本 */
static bool                Can_Initialized = false;
static Can_ControllerStateType Can_State[CAN_CONTROLLER_MAX] = { CAN_CS_UNINIT };
static Can_ErrorStateType      Can_ErrorState[CAN_CONTROLLER_MAX] = { CAN_ERRORSTATE_ACTIVE };

/* TX/RX Mailbox 数量 (来自 MCAL 配置) */
static uint8_t Can_TxCount = 0U;
static uint8_t Can_RxCount = 0U;

/* ===================================================================
 *  内部: MCAL 配置 → PAL 配置 转换
 * =================================================================== */

static void Can_BuildPalConfig(const Can_ConfigType *mcal,
                               can_user_config_t  *pal)
{
    pal->maxBuffNum  = mcal->max_num_mb;
    pal->mode        = (mcal->flexcan_mode == CAN_MODE_LOOPBACK)
                       ? CAN_LOOPBACK_MODE : CAN_NORMAL_MODE;
    pal->peClkSrc    = CAN_CLK_SOURCE_OSC;      /* PE 直连 OSC (匹配卖家配置) */
    pal->enableFD    = false;
    pal->payloadSize = CAN_PAYLOAD_SIZE_8;

    /* 位时序 — 使用卖家已验证的 13TQ 默认，后续可从 MCAL 配置覆盖 */
    pal->nominalBitrate.propSeg    = mcal->prop_seg;
    pal->nominalBitrate.phaseSeg1  = mcal->phase_seg1;
    pal->nominalBitrate.phaseSeg2  = mcal->phase_seg2;
    pal->nominalBitrate.preDivider = mcal->pre_divider;
    pal->nominalBitrate.rJumpwidth = mcal->r_jumpwidth;

    /* FD 数据相位时序 (不使用 FD, 与 nominal 相同即可) */
    pal->dataBitrate = pal->nominalBitrate;
    pal->extension   = NULL;
}

/* ===================================================================
 *  Can_Init
 * =================================================================== */

Std_ReturnType Can_Init(const Can_ConfigType *ConfigPtr)
{
    uint8_t i;

    if (ConfigPtr == NULL) return E_NOT_OK;

    /* 保存 MCAL 配置副本 */
    Can_Config = *ConfigPtr;
    Can_TxCount = ConfigPtr->num_tx_mailboxes;
    Can_RxCount = ConfigPtr->num_rx_mailboxes;

    /* MCAL → PAL 转换 */
    Can_BuildPalConfig(ConfigPtr, &Can_PalConfig);

    /* 通过 CAN PAL 初始化 */
    if (CAN_Init(&Can_Instance, &Can_PalConfig) != STATUS_SUCCESS) {
        return E_NOT_OK;
    }

    /* 统一配置 TX/RX MB 共用参数 */
    Can_TxBuffCfg = (can_buff_config_t){
        .enableFD  = false,
        .enableBRS = false,
        .fdPadding = 0U,
        .idType    = CAN_MSG_ID_STD,
        .isRemote  = false,
    };
    Can_RxBuffCfg = Can_TxBuffCfg;  /* 初始一样 */

    /* 配置 TX Mailboxes */
    for (i = 0U; i < Can_TxCount; i++) {
        CAN_ConfigTxBuff(&Can_Instance, i, &Can_TxBuffCfg);
    }

    /* 配置 RX Mailboxes (索引从 num_tx 开始) */
    for (i = 0U; i < Can_RxCount; i++) {
        uint32_t rx_id = Can_Config.rx_mailboxes[i].id;
        CAN_ConfigRxBuff(&Can_Instance, Can_TxCount + i,
                         &Can_RxBuffCfg, rx_id);
    }

    Can_Initialized = true;
    Can_State[Can_Config.controller] = CAN_CS_STOPPED;
    Can_ErrorState[Can_Config.controller] = CAN_ERRORSTATE_ACTIVE;
    return E_OK;
}

/* ===================================================================
 *  Can_DeInit
 * =================================================================== */

Std_ReturnType Can_DeInit(void)
{
    if (!Can_Initialized) return E_NOT_OK;
    (void)CAN_Deinit(&Can_Instance);
    Can_Initialized = false;
    Can_State[Can_Config.controller] = CAN_CS_UNINIT;
    return E_OK;
}

/* ===================================================================
 *  Can_SetControllerMode  (SWS_Can_00098)
 * =================================================================== */

Std_ReturnType Can_SetControllerMode(Can_ControllerType     Controller,
                                     Can_ControllerStateType Transition)
{
    if (Controller >= CAN_CONTROLLER_MAX || !Can_Initialized) {
        return E_NOT_OK;
    }

    if (Transition == CAN_CS_STARTED) {
        Can_State[Controller] = CAN_CS_STARTED;
    } else if (Transition == CAN_CS_STOPPED) {
        Can_State[Controller] = CAN_CS_STOPPED;
    } else {
        return E_NOT_OK;
    }
    return E_OK;
}

/* ===================================================================
 *  Can_Write  (SWS_Can_00106)
 *  @note  AUTOSAR 标准签名: 只传 HTH, 不传 Controller。
 *         Controller 由 Can_Config.controller 内部维护。
 *         每次发送前必须调用 CAN_ConfigTxBuff 重新配置 TX MB。
 * =================================================================== */

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    can_message_t tx_msg;
    uint8_t controller = Can_Config.controller;

    if (!Can_Initialized)                        return E_NOT_OK;
    if (Can_State[controller] != CAN_CS_STARTED) return E_NOT_OK;
    if (Hth >= Can_TxCount)                      return E_NOT_OK;
    if (PduInfo == NULL)                         return E_NOT_OK;
    if (PduInfo->length > 8U)                    return E_NOT_OK;

    /* 每次发送前重配 TX MB — 清除上次发送残留状态 */
    CAN_ConfigTxBuff(&Can_Instance, Hth, &Can_TxBuffCfg);

    /* 构造 PAL 消息 */
    tx_msg.cs     = 0U;
    tx_msg.id     = PduInfo->id;
    tx_msg.length = PduInfo->length;
    for (uint8_t i = 0U; i < PduInfo->length; i++) {
        tx_msg.data[i] = PduInfo->data[i];
    }

    return CAN_STATUS_TO_STD_RET(CAN_Send(&Can_Instance, Hth, &tx_msg));
}

/* ===================================================================
 *  Can_Read  (项目扩展 — 轮询模式，AUTOSAR 标准使用中断回调)
 *  保持原有签名，便于现有代码兼容
 * =================================================================== */

status_t Can_Read(uint8_t Controller, uint8_t Hrh,
                  Can_PduType *PduInfo)
{
    can_message_t rx_msg;
    status_t      ret;

    if (!Can_Initialized)                        return STATUS_ERROR;
    if (Controller != Can_Config.controller)     return STATUS_ERROR;
    if (Can_State[Controller] != CAN_CS_STARTED) return STATUS_ERROR;
    if (Hrh < Can_TxCount)                       return STATUS_ERROR;
    if (PduInfo == NULL)                         return STATUS_ERROR;

    /* 实际 MB 索引 = TX 数量 + RX 偏移 */
    ret = CAN_Receive(&Can_Instance, Hrh, &rx_msg);
    if (ret == STATUS_SUCCESS) {
        PduInfo->id          = rx_msg.id;
        PduInfo->length      = rx_msg.length;
        PduInfo->is_extended = (rx_msg.cs & 0x1U) ? true : false;  /* IDE bit */
        PduInfo->is_remote   = false;
        for (uint8_t i = 0U; i < 8U; i++) {
            PduInfo->data[i] = (i < rx_msg.length) ? rx_msg.data[i] : 0U;
        }
    }
    return ret;
}

/* ===================================================================
 *  Can_GetControllerErrorState  (SWS_Can_00167)
 * =================================================================== */

Std_ReturnType Can_GetControllerErrorState(Can_ControllerType  Controller,
                                           Can_ErrorStateType *ErrorStatePtr)
{
    if (Controller >= CAN_CONTROLLER_MAX || !Can_Initialized) {
        return E_NOT_OK;
    }
    if (ErrorStatePtr == NULL) {
        return E_NOT_OK;
    }
    *ErrorStatePtr = Can_ErrorState[Controller];
    return E_OK;
}

/* ===================================================================
 *  Can_GetControllerMode  (SWS_Can_00130)
 * =================================================================== */

Std_ReturnType Can_GetControllerMode(Can_ControllerType      Controller,
                                     Can_ControllerStateType *ModePtr)
{
    if (Controller >= CAN_CONTROLLER_MAX || !Can_Initialized) {
        return E_NOT_OK;
    }
    if (ModePtr == NULL) {
        return E_NOT_OK;
    }
    *ModePtr = Can_State[Controller];
    return E_OK;
}