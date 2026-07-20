/**
 * @file    CanIf.c
 * @brief   [AUTOSAR CP] CAN Interface 实现 — ECU Abstraction 层
 *
 * @note    对标 AUTOSAR SWS_CanIf:
 *          - SWS_CanIf_00050: CanIf_Transmit    → PDU 查表 → Can_Write
 *          - SWS_CanIf_00030: CanIf_RxIndication → CAN 帧到达通知
 *          - SWS_CanIf_00040: CanIf_TxConfirmation → 发送完成通知
 *
 *          当前 TX HTH=0, RX HRH=1 (与 main.c 硬件 mailbox 配置一致)
 */

#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "CanIf_PduId.h"
#include "Can.h"
#include "Log.h"

/* ===================================================================
 *  CAN Interface 模块 ID (AUTOSAR DET ModuleId)
 *  CanIf ModuleId = 0x32 (50)
 * =================================================================== */
#define CANIF_MODULE_ID   0x32U

/* ===================================================================
 *  API ID (配合 DET 定位错误来源)
 * =================================================================== */
#define CANIF_INIT_ID          0x00U
#define CANIF_TRANSMIT_ID      0x01U
#define CANIF_RX_INDICATION_ID 0x02U
#define CANIF_TX_CONFIRM_ID    0x03U

/* ===================================================================
 *  错误 ID
 * =================================================================== */
#define CANIF_E_PARAM          0x01U   /* 参数错误 (NULL pointer)    */
#define CANIF_E_UNINIT         0x02U   /* 模块未初始化              */
#define CANIF_E_INVALID_PDU_ID 0x03U   /* 无效的 PDU ID            */

/* ===================================================================
 *  HTH / HRH (Hardware Transmit/Receive Handle)
 *  当前写死与 main.c 的 TX_MB=0 / RX_MB=1 一致
 *  后续可移入 CanIf_PduConfigType 扩展字段
 * =================================================================== */
#define CANIF_TX_HTH  0U
#define CANIF_RX_HRH  1U

/* ===================================================================
 *  模块状态
 * =================================================================== */
static uint8_t canif_state = 0U;         /* 0=未初始化, 1=已初始化 */

/* ===================================================================
 *  内部辅助函数
 * =================================================================== */

/**
 * @brief 按 PDU ID 查找 CanIf_PduConfig 条目
 * @return 找到返回指针，否则返回 NULL
 */
static const CanIf_PduConfigType *CanIf_FindConfigByPduId(CanIf_PduIdType PduId)
{
    for (uint8_t i = 0U; i < CanIf_PduConfig_Count; i++) {
        if (CanIf_PduConfig[i].pdu_id == PduId) {
            return &CanIf_PduConfig[i];
        }
    }
    return NULL;
}

/**
 * @brief 按 CAN ID 查找 PDU ID（RX 路径：CAN 帧 ID → PDU ID）
 * @return 找到返回 pdu_id，否则返回 CANIF_PDU_COUNT（=无效值）
 */
CanIf_PduIdType CanIf_FindPduIdByCanId(uint32_t CanId)
{
    for (uint8_t i = 0U; i < CanIf_PduConfig_Count; i++) {
        if (CanIf_PduConfig[i].can_id == CanId) {
            return CanIf_PduConfig[i].pdu_id;
        }
    }
    return (CanIf_PduIdType)CANIF_PDU_COUNT;  /* 无效值 */
}

/* ===================================================================
 *  API 实现
 * =================================================================== */

void CanIf_Init(void)
{
    canif_state = 1U;
    LOG_I("CanIf", "Init done, %u controller(s), %u PDU(s)",
          (unsigned int)CANIF_CONTROLLER_COUNT,
          (unsigned int)CanIf_PduConfig_Count);
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

    /* 校验 PDU ID 是否已配置 */
    const CanIf_PduConfigType *cfg = CanIf_FindConfigByPduId(PduPtr->id);
    if (cfg == NULL) {
        LOG_E("CanIf", "RxIndication: invalid PduId=%u (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)PduPtr->id, (unsigned int)CANIF_MODULE_ID,
              (unsigned int)CANIF_RX_INDICATION_ID, (unsigned int)CANIF_E_INVALID_PDU_ID);
        return;
    }

    (void)Controller;

    LOG_D("CanIf", "RX PDU %u (CAN 0x%lX), len=%u",
          (unsigned int)PduPtr->id, (unsigned long)cfg->can_id, (unsigned int)PduPtr->length);

    /* TODO: Step 2 — 转发给 PduR_CanIfRxIndication(PduPtr->id, &pduInfo) */
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

    LOG_D("CanIf", "TX confirm PDU %u", (unsigned int)PduPtr->id);

    /* TODO: Step 2 — 转发给 PduR_CanIfTxConfirmation(PduPtr->id) */
}

uint8_t CanIf_Transmit(CanIf_ControllerType Controller, CanIf_PduType *PduPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "Transmit: not initialized (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TRANSMIT_ID,
              (unsigned int)CANIF_E_UNINIT);
        return E_NOT_OK;
    }
    if (PduPtr == NULL) {
        LOG_E("CanIf", "Transmit: NULL PduPtr (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TRANSMIT_ID,
              (unsigned int)CANIF_E_PARAM);
        return E_NOT_OK;
    }
#else
    (void)PduPtr;
#endif

    /* 1. 查 CanIf_PduConfig[] 表，校验 PDU ID */
    const CanIf_PduConfigType *cfg = CanIf_FindConfigByPduId(PduPtr->id);
    if (cfg == NULL) {
        LOG_E("CanIf", "Transmit: invalid PduId=%u (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)PduPtr->id, (unsigned int)CANIF_MODULE_ID,
              (unsigned int)CANIF_TRANSMIT_ID, (unsigned int)CANIF_E_INVALID_PDU_ID);
        return E_NOT_OK;
    }

    /* 2. 构造 MCAL Can_PduType (CanIf → Can 格式转换) */
    Can_PduType canPdu = {0};  /* 全量零初始化 is_extended/is_remote */
    canPdu.id     = cfg->can_id;
    canPdu.length = PduPtr->length;
    {
        uint8_t len = (PduPtr->length > 8U) ? 8U : PduPtr->length;
        for (uint8_t i = 0U; i < len; i++) {
            canPdu.data[i] = PduPtr->data[i];
        }
    }

    /* 3. 调用 MCAL Can_Write 发送 */
    status_t ret = Can_Write(Controller, CANIF_TX_HTH, &canPdu);

    if (ret != STATUS_SUCCESS) {
        LOG_E("CanIf", "Transmit: Can_Write failed (PduId=%u, CanId=0x%lX, ret=%d)",
              (unsigned int)PduPtr->id, (unsigned long)cfg->can_id, (int)ret);
        return E_NOT_OK;
    }

    LOG_D("CanIf", "TX PDU %u (CAN 0x%lX), len=%u",
          (unsigned int)PduPtr->id, (unsigned long)cfg->can_id, (unsigned int)PduPtr->length);

    return E_OK;
}
