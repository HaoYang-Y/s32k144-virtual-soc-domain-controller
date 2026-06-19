/**
 * @file    CanIf.c
 * @brief   [SKELETON] CAN Interface 实现 — 含开发错误检测 (DET/LOG_E)
 *
 * @todo    实现 PDU 收发: CanIf_Transmit → Can_Write
 *          CanIf_RxIndication → 转发给 PduR
 */

#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "Log.h"
/* TODO: #include "Can.h"   #include "PduR.h"  */

/* ===================================================================
 *  CAN Interface 模块 ID (用于 DET 错误报告)
 *  对应 AUTOSAR 规范中 CanIf ModuleId = 0x32 (50)
 * =================================================================== */
#define CANIF_MODULE_ID   0x32U

/* ===================================================================
 *  API ID (用于 DET 错误报告，配合模块 ID 定位错误来源)
 * =================================================================== */
#define CANIF_INIT_ID          0x00U
#define CANIF_TRANSMIT_ID      0x01U
#define CANIF_RX_INDICATION_ID 0x02U
#define CANIF_TX_CONFIRM_ID    0x03U

/* ===================================================================
 *  错误 ID
 * =================================================================== */
#define CANIF_E_PARAM          0x01U   /* 参数错误 (NULL pointer) */
#define CANIF_E_UNINIT         0x02U   /* 模块未初始化         */

/* ===================================================================
 *  模块状态
 * =================================================================== */
static uint8_t canif_state = 0U;         /* 0=未初始化, 1=已初始化 */

void CanIf_Init(void)
{
    canif_state = 1U;
    LOG_I("CanIf", "Init done, %u controller(s)", (unsigned int)CANIF_CONTROLLER_COUNT);
}

void CanIf_RxIndication(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "RxIndication: not initialized (Mod=0x%02X Api=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_RX_INDICATION_ID);
        return;
    }
    if (PduPtr == NULL) {
        LOG_E("CanIf", "RxIndication: NULL PduPtr (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_RX_INDICATION_ID,
              (unsigned int)CANIF_E_PARAM);
        return;
    }
#else
    (void)PduPtr;
#endif
    (void)Controller;
    /* TODO: 将接收到的 PDU 转发给 PduR_CanIfRxIndication */
}

void CanIf_TxConfirmation(CanIf_ControllerType Controller, const CanIf_PduType *PduPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "TxConfirm: not initialized (Mod=0x%02X Api=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TX_CONFIRM_ID);
        return;
    }
    if (PduPtr == NULL) {
        LOG_E("CanIf", "TxConfirm: NULL PduPtr (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TX_CONFIRM_ID,
              (unsigned int)CANIF_E_PARAM);
        return;
    }
#else
    (void)PduPtr;
#endif
    (void)Controller;
    /* TODO: 通知 PduR 发送完成 */
}

uint8_t CanIf_Transmit(CanIf_ControllerType Controller, CanIf_PduType *PduPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "Transmit: not initialized (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TRANSMIT_ID,
              (unsigned int)CANIF_E_UNINIT);
        return 1U;
    }
    if (PduPtr == NULL) {
        LOG_E("CanIf", "Transmit: NULL PduPtr (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TRANSMIT_ID,
              (unsigned int)CANIF_E_PARAM);
        return 1U;
    }
#else
    (void)PduPtr;
#endif
    (void)Controller;
    /* TODO: 调用 Can_Write 发送 */
    return 0U;
}
