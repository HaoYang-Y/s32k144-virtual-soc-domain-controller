/**
 * @file    Can.c
 * @brief   AUTOSAR CP MCAL Can 驱动 — 多路 CAN 完整实现
 *
 * @note    HTH 编码: bit[15:8]=Controller  bit[7:0]=MB Index
 *          支持最多 CAN_CONTROLLER_MAX 路 FlexCAN 并行工作。
 *          每次 Can_Write 前必须重配 TX MB (CAN_ConfigTxBuff)，
 *          否则 FLEXCAN_DRV_Send 的 MB 状态检查会返回 BUSY。
 */

#include "Can.h"
#include "Can_Cfg.h"
#include "can_pal.h"
#include "can_pal_mapping.h"
#include "can_pal_cfg.h"
#include <stddef.h>

/* ===================================================================
 *  宏: status_t ↔ Std_ReturnType 映射
 * =================================================================== */

#define CAN_STATUS_TO_STD_RET(s)  ((s) == STATUS_SUCCESS ? E_OK : E_NOT_OK)

/* ===================================================================
 *  每控制器状态
 * =================================================================== */

typedef struct {
    can_instance_t            instance;      /* PAL 实例 (FlexCAN0/1/2) */
    Can_ConfigType            config;        /* MCAL 配置副本 */
    can_user_config_t         palConfig;     /* PAL 配置副本 */
    can_buff_config_t         txBuffCfg;     /* TX MB 缓冲配置 */
    can_buff_config_t         rxBuffCfg;     /* RX MB 缓冲配置 */
    bool                      initialized;   /* 是否已完成 Can_Init */
    Can_ControllerStateType   state;         /* 运行状态 */
    Can_ErrorStateType        errorState;    /* 错误状态 */
    uint8_t                   txCount;       /* TX MB 数量 */
    uint8_t                   rxCount;       /* RX MB 数量 */

    volatile bool             txComplete[16]; /* ISR 标记 TX 完成 */
    /* RX 中断相关 — 静态 buffer，ISR 写入 */
    can_message_t             rxMsgBuf[16];
    volatile bool             rxPending[16];
} Can_CtrlState;

static Can_CtrlState Can_Ctrl[CAN_CONTROLLER_MAX];

static Can_RxNotificationType Can_RxCallback = NULL;
static Can_TxConfirmationType Can_TxCallback = NULL;

/* ===================================================================
 *  内部: MCAL 配置 → PAL 配置 转换
 * =================================================================== */

static void Can_BuildPalConfig(const Can_ConfigType *mcal,
                               can_user_config_t  *pal)
{
    pal->maxBuffNum  = mcal->max_num_mb;
    pal->mode        = (mcal->flexcan_mode == CAN_MODE_LOOPBACK)
                       ? CAN_LOOPBACK_MODE : CAN_NORMAL_MODE;
    pal->peClkSrc    = CAN_CLK_SOURCE_OSC;
    pal->enableFD    = false;
    pal->payloadSize = CAN_PAYLOAD_SIZE_8;

    pal->nominalBitrate.propSeg    = mcal->prop_seg;
    pal->nominalBitrate.phaseSeg1  = mcal->phase_seg1;
    pal->nominalBitrate.phaseSeg2  = mcal->phase_seg2;
    pal->nominalBitrate.preDivider = mcal->pre_divider;
    pal->nominalBitrate.rJumpwidth = mcal->r_jumpwidth;

    pal->dataBitrate = pal->nominalBitrate;
    pal->extension   = NULL;
}

/* ===================================================================
 *  Can_Init  — 初始化指定控制器
 * =================================================================== */

Std_ReturnType Can_Init(Can_ControllerType Controller, const Can_ConfigType *ConfigPtr)
{
    if (Controller >= CAN_CONTROLLER_MAX) return E_NOT_OK;
    if (ConfigPtr == NULL)               return E_NOT_OK;

    Can_CtrlState *c = &Can_Ctrl[Controller];

    c->config = *ConfigPtr;
    c->txCount = ConfigPtr->num_tx_mailboxes;
    c->rxCount = ConfigPtr->num_rx_mailboxes;

    c->instance.instType = CAN_INST_TYPE_FLEXCAN;
    c->instance.instIdx  = (uint32_t)Controller;

    Can_BuildPalConfig(ConfigPtr, &c->palConfig);

    if (CAN_Init(&c->instance, &c->palConfig) != STATUS_SUCCESS) {
        return E_NOT_OK;
    }

    c->txBuffCfg = (can_buff_config_t){
        .enableFD  = false, .enableBRS = false, .fdPadding = 0U,
        .idType    = CAN_MSG_ID_STD, .isRemote  = false,
    };
    c->rxBuffCfg = c->txBuffCfg;

    for (uint8_t i = 0U; i < c->txCount; i++) {
        CAN_ConfigTxBuff(&c->instance, i, &c->txBuffCfg);
    }

    for (uint8_t i = 0U; i < c->rxCount; i++) {
        uint32_t rx_id = c->config.rx_mailboxes[i].id;
        CAN_ConfigRxBuff(&c->instance, c->txCount + i, &c->rxBuffCfg, rx_id);
    }

    c->initialized = true;
    c->state       = CAN_CS_STOPPED;
    c->errorState  = CAN_ERRORSTATE_ACTIVE;
    return E_OK;
}

/* ===================================================================
 *  Can_DeInit
 * =================================================================== */

Std_ReturnType Can_DeInit(void)
{
    for (uint8_t i = 0U; i < CAN_CONTROLLER_MAX; i++) {
        Can_CtrlState *c = &Can_Ctrl[i];
        if (c->initialized) {
            (void)CAN_Deinit(&c->instance);
            c->initialized = false;
            c->state = CAN_CS_UNINIT;
        }
    }
    return E_OK;
}

/* ===================================================================
 *  Can_SetControllerMode
 * =================================================================== */

Std_ReturnType Can_SetControllerMode(Can_ControllerType     Controller,
                                     Can_ControllerStateType Transition)
{
    if (Controller >= CAN_CONTROLLER_MAX)   return E_NOT_OK;
    Can_CtrlState *c = &Can_Ctrl[Controller];
    if (!c->initialized)                    return E_NOT_OK;

    /* AUTOSAR 状态机校验: 仅允许合法跳转 */
    switch (Transition) {
    case CAN_CS_STARTED:
        if (c->state != CAN_CS_STOPPED) {
            return E_NOT_OK;  /* STARTED 只能从 STOPPED 跳入 */
        }
        c->state = CAN_CS_STARTED;
        break;

    case CAN_CS_STOPPED:
        if (c->state != CAN_CS_STARTED) {
            return E_NOT_OK;  /* STOPPED 只能从 STARTED 跳入 */
        }
        /* 停止控制器: 中止所有挂起传输 + 暂停 RX MB */
        for (uint8_t i = 0U; i < c->txCount; i++) {
            (void)CAN_AbortTransfer(&c->instance, i);
        }
        c->state = CAN_CS_STOPPED;
        break;

    default:
        /* CAN_CS_SLEEP / CAN_CS_UNINIT 等模式本学习项目不支持 */
        return E_NOT_OK;
    }
    return E_OK;
}

/* ===================================================================
 *  Can_Write  — HTH 解码: bit[15:8]=Controller  bit[7:0]=MB
 * =================================================================== */

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
    uint8_t ctrl   = CAN_HTH_CTRL(Hth);
    uint8_t mb_idx = CAN_HTH_MB(Hth);

    if (ctrl >= CAN_CONTROLLER_MAX) return E_NOT_OK;
    Can_CtrlState *c = &Can_Ctrl[ctrl];
    if (!c->initialized)                     return E_NOT_OK;
    if (c->state != CAN_CS_STARTED)          return E_NOT_OK;
    if (mb_idx >= c->txCount)               return E_NOT_OK;
    if (PduInfo == NULL)                     return E_NOT_OK;
    if (PduInfo->length > 8U)                return E_NOT_OK;

    (void)CAN_AbortTransfer(&c->instance, mb_idx);
    CAN_ConfigTxBuff(&c->instance, mb_idx, &c->txBuffCfg);

    can_message_t tx_msg;
    tx_msg.cs     = 0U;
    tx_msg.id     = PduInfo->id;
    tx_msg.length = PduInfo->length;
    for (uint8_t i = 0U; i < PduInfo->length; i++) {
        tx_msg.data[i] = PduInfo->data[i];
    }

    return CAN_STATUS_TO_STD_RET(CAN_Send(&c->instance, mb_idx, &tx_msg));
}

/* ===================================================================
 *  Can_Read  — 轮询模式 (中断模式下由 Can_MainFunctionRx 内部调用)
 * =================================================================== */

status_t Can_Read(uint8_t Controller, uint8_t Hrh,
                  Can_PduType *PduInfo)
{
    if (Controller >= CAN_CONTROLLER_MAX) return STATUS_ERROR;
    Can_CtrlState *c = &Can_Ctrl[Controller];
    if (!c->initialized)                  return STATUS_ERROR;
    if (c->state != CAN_CS_STARTED)       return STATUS_ERROR;
    if (Hrh < c->txCount)                 return STATUS_ERROR;
    if (PduInfo == NULL)                  return STATUS_ERROR;

    can_message_t rx_msg;
    status_t ret = CAN_Receive(&c->instance, Hrh, &rx_msg);
    if (ret == STATUS_SUCCESS) {
        PduInfo->id          = rx_msg.id;
        PduInfo->length      = rx_msg.length;
        PduInfo->is_extended = (rx_msg.cs & 0x1U) ? true : false;
        PduInfo->is_remote   = false;
        for (uint8_t i = 0U; i < 8U; i++) {
            PduInfo->data[i] = (i < rx_msg.length) ? rx_msg.data[i] : 0U;
        }
    }
    return ret;
}

/* ===================================================================
 *  Can_GetControllerErrorState / Can_GetControllerMode
 *
 *  注: GetControllerErrorState 返回软件缓存的错误状态。
 *  生产级实现应读取 FlexCAN ESR1 寄存器的 FLTCONF 字段:
 *    00 = Error Active, 01 = Error Passive, 1x = Bus Off
 *  (AUTOSAR SWS_Can_00167 要求从硬件读取实时状态)
 * =================================================================== */

Std_ReturnType Can_GetControllerErrorState(Can_ControllerType  Controller,
                                           Can_ErrorStateType *ErrorStatePtr)
{
    if (Controller >= CAN_CONTROLLER_MAX)   return E_NOT_OK;
    Can_CtrlState *c = &Can_Ctrl[Controller];
    if (!c->initialized)                    return E_NOT_OK;
    if (ErrorStatePtr == NULL)              return E_NOT_OK;
    *ErrorStatePtr = c->errorState;
    return E_OK;
}

Std_ReturnType Can_GetControllerMode(Can_ControllerType      Controller,
                                     Can_ControllerStateType *ModePtr)
{
    if (Controller >= CAN_CONTROLLER_MAX)   return E_NOT_OK;
    Can_CtrlState *c = &Can_Ctrl[Controller];
    if (!c->initialized)                    return E_NOT_OK;
    if (ModePtr == NULL)                    return E_NOT_OK;
    *ModePtr = c->state;
    return E_OK;
}

/* ===================================================================
 *  中断模式 — 每控制器独立 ISR 回调 + 武装 RX MB
 * =================================================================== */

static void Can_IrqCallback(uint32_t instance, can_event_t eventType,
                             uint32_t buffIdx, void *driverState)
{
    (void)driverState;

    if (instance >= CAN_CONTROLLER_MAX || buffIdx >= 16U) return;

    if (eventType == CAN_EVENT_RX_COMPLETE) {
        Can_Ctrl[instance].rxPending[buffIdx] = true;
    } else if (eventType == CAN_EVENT_TX_COMPLETE) {
        Can_Ctrl[instance].txComplete[buffIdx] = true;
    }
}

void Can_EnableInterrupts(void)
{
    for (uint8_t ctrl = 0U; ctrl < CAN_CONTROLLER_MAX; ctrl++) {
        Can_CtrlState *c = &Can_Ctrl[ctrl];
        if (!c->initialized) continue;

        (void)CAN_InstallEventCallback(&c->instance, Can_IrqCallback, NULL);

        for (uint8_t i = 0U; i < c->rxCount; i++) {
            uint8_t mb_idx = c->txCount + i;
            c->rxMsgBuf[mb_idx].cs = 0U;
            c->rxPending[mb_idx]   = false;
            (void)CAN_Receive(&c->instance, mb_idx, &c->rxMsgBuf[mb_idx]);
        }
    }
}

void Can_RegisterRxCallback(Can_RxNotificationType callback)
{
    Can_RxCallback = callback;
}

void Can_RegisterTxCallback(Can_TxConfirmationType callback)
{
    Can_TxCallback = callback;
}

bool Can_MainFunctionRx(void)
{
    bool got_frame = false;
    if (Can_RxCallback == NULL) return false;

    for (uint8_t ctrl = 0U; ctrl < CAN_CONTROLLER_MAX; ctrl++) {
        Can_CtrlState *c = &Can_Ctrl[ctrl];
        if (!c->initialized) continue;

        for (uint8_t i = 0U; i < c->rxCount; i++) {
            uint8_t mb_idx = c->txCount + i;
            if (!c->rxPending[mb_idx]) continue;
            c->rxPending[mb_idx] = false;
            got_frame = true;

            can_message_t *rx = &c->rxMsgBuf[mb_idx];
            Can_PduType pdu = {
                .id          = rx->id,
                .length      = rx->length,
                .is_extended = (rx->cs & 0x1U) ? true : false,
                .is_remote   = false,
            };
            Can_RxCallback((Can_ControllerType)ctrl, mb_idx, &pdu, rx->data);

            c->rxMsgBuf[mb_idx].cs = 0U;
            (void)CAN_Receive(&c->instance, mb_idx, &c->rxMsgBuf[mb_idx]);
        }
    }
    return got_frame;
}

void Can_MainFunctionWrite(void)
{
    for (uint8_t ctrl = 0U; ctrl < CAN_CONTROLLER_MAX; ctrl++) {
        Can_CtrlState *c = &Can_Ctrl[ctrl];
        if (!c->initialized) continue;

        for (uint8_t i = 0U; i < c->txCount; i++) {
            if (c->txComplete[i]) {
                c->txComplete[i] = false;
                /* 通知上层 (CanIf) TX 完成 — 由上层翻译为 PDU ID 后确认 */
                if (Can_TxCallback != NULL) {
                    Can_TxCallback((Can_ControllerType)ctrl, i);
                }
            }
        }
    }
}
