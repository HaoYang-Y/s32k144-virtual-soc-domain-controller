/**
 * @file    CanIf.c
 * @brief   [AUTOSAR CP] CAN Interface 实现 — ECU Abstraction 层
 *
 * @note    对标 AUTOSAR SWS_CanIf:
 *          - SWS_CanIf_00221: CanIf_Transmit     → PDU 查表 → Can_Write
 *          - SWS_CanIf_00204: CanIf_RxIndication  → CAN 帧到达通知
 *          - SWS_CanIf_00211: CanIf_TxConfirmation → 发送完成通知
 *
 *          PduInfoType → Can_PduType 的格式转换发生在此层：
 *            N-PDU (PduInfoType, 指针+长度) → L-PDU (Can_PduType, 固定8字节)
 *
 *          每个 TX PDU 的 HTH 存于 CanIf_PduConfig[].hth (由生成脚本写入)。
 */

#include "CanIf.h"
#include "CanIf_Cfg.h"
#include "CanIf_PduId.h"
#include "Can.h"
#include "PduR.h"
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
static const CanIf_PduConfigType *CanIf_FindConfigByPduId(uint16_t PduId)
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

/* MCAL Can RX 回调 — PDU ID 翻译后调用 CanIf_RxIndication */
static void CanIf_McalRxCallback(Can_ControllerType Controller,
                                  uint8_t Hrh, const Can_PduType *PduInfo,
                                  const uint8_t *data)
{
    (void)Controller;
    (void)Hrh;
    CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(PduInfo->id);
    if (pduId < CANIF_PDU_COUNT) {
        PduInfoType rxPdu = {
            .SduId      = pduId,
            .SduLength  = PduInfo->length,
            .SduDataPtr = (uint8_t *)data,
        };
        CanIf_RxIndication(pduId, &rxPdu);
    }
}

/* MCAL Can TX 回调 — (Controller, MB) 反查 PDU ID 后调用 CanIf_TxConfirmation */
static void CanIf_McalTxCallback(Can_ControllerType Controller, uint8_t MbIndex)
{
    Can_HwHandleType hth = CAN_HTH_MAKE(Controller, MbIndex);

    for (uint8_t i = 0U; i < CanIf_PduConfig_Count; i++) {
        if (CanIf_PduConfig[i].hth == hth) {
            CanIf_TxConfirmation(CanIf_PduConfig[i].pdu_id);
            return;
        }
    }
    LOG_W("CanIf", "TX confirm: no PDU for HTH=0x%04X", (unsigned int)hth);
}

void CanIf_Init(void)
{
    Can_RegisterRxCallback(CanIf_McalRxCallback);
    Can_RegisterTxCallback(CanIf_McalTxCallback);

    canif_state = 1U;
    LOG_I("CanIf", "Init done, %u controller(s), %u PDU(s)",
          (unsigned int)CANIF_CONTROLLER_COUNT,
          (unsigned int)CanIf_PduConfig_Count);
}

void CanIf_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "RxIndication: not initialized (Mod=0x%02X Api=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_RX_INDICATION_ID);
        return;
    }
    if (PduInfoPtr == NULL) {
        LOG_E("CanIf", "RxIndication: NULL PduInfoPtr (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_RX_INDICATION_ID,
              (unsigned int)CANIF_E_PARAM);
        return;
    }
#else
    (void)PduInfoPtr;
#endif

    /* 校验 PDU ID 是否已配置 */
    const CanIf_PduConfigType *cfg = CanIf_FindConfigByPduId(RxPduId);
    if (cfg == NULL) {
        LOG_E("CanIf", "RxIndication: invalid PduId=%u (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)RxPduId, (unsigned int)CANIF_MODULE_ID,
              (unsigned int)CANIF_RX_INDICATION_ID, (unsigned int)CANIF_E_INVALID_PDU_ID);
        return;
    }

    /* LOG_D 在 ISR 上下文中调用会阻塞 UART — 需要调试时手动开启 */
#if 0
    LOG_D("CanIf", "RX PDU %u (CAN 0x%lX), len=%u",
          (unsigned int)RxPduId, (unsigned long)cfg->can_id,
          (unsigned int)PduInfoPtr->SduLength);
#endif

    /* 转发给 PduR → CanTp 进行 N-PDU 重组 */
    PduR_CanIfRxIndication(RxPduId, PduInfoPtr);
}

void CanIf_TxConfirmation(PduIdType TxPduId)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "TxConfirm: not initialized (Mod=0x%02X Api=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TX_CONFIRM_ID);
        return;
    }
#else
    (void)TxPduId;
#endif

    LOG_D("CanIf", "TX confirm PDU %u", (unsigned int)TxPduId);

    /* 转发给 PduR → CanTp 进行流控状态推进 */
    PduR_CanIfTxConfirmation(TxPduId);
}

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
#if (CANIF_DEV_ERROR_DETECT == STD_ON)
    if (canif_state == 0U) {
        LOG_E("CanIf", "Transmit: not initialized (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TRANSMIT_ID,
              (unsigned int)CANIF_E_UNINIT);
        return E_NOT_OK;
    }
    if (PduInfoPtr == NULL) {
        LOG_E("CanIf", "Transmit: NULL PduInfoPtr (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)CANIF_MODULE_ID, (unsigned int)CANIF_TRANSMIT_ID,
              (unsigned int)CANIF_E_PARAM);
        return E_NOT_OK;
    }
#else
    (void)PduInfoPtr;
#endif

    /* 1. 查 CanIf_PduConfig[] 表，校验 PDU ID */
    const CanIf_PduConfigType *cfg = CanIf_FindConfigByPduId(TxPduId);
    if (cfg == NULL) {
        LOG_E("CanIf", "Transmit: invalid PduId=%u (Mod=0x%02X Api=0x%02X Err=0x%02X)",
              (unsigned int)TxPduId, (unsigned int)CANIF_MODULE_ID,
              (unsigned int)CANIF_TRANSMIT_ID, (unsigned int)CANIF_E_INVALID_PDU_ID);
        return E_NOT_OK;
    }

    /* 2. 构造 MCAL Can_PduType (N-PDU → L-PDU 格式转换) */
    Can_PduType canPdu = {0};  /* 全量零初始化 is_extended/is_remote */
    canPdu.id     = cfg->can_id;
    canPdu.length = PduInfoPtr->SduLength;
    {
        uint8_t len = (PduInfoPtr->SduLength > 8U) ? 8U : PduInfoPtr->SduLength;
        for (uint8_t i = 0U; i < len; i++) {
            canPdu.data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }

    /* 3. 调用 MCAL Can_Write (AUTOSAR 标准签名: 只传 HTH, 来自配置表) */
    Std_ReturnType ret = Can_Write(cfg->hth, &canPdu);

    if (ret != E_OK) {
        LOG_E("CanIf", "Transmit: Can_Write failed (PduId=%u, CanId=0x%lX)",
              (unsigned int)TxPduId, (unsigned long)cfg->can_id);
        return E_NOT_OK;
    }

    LOG_D("CanIf", "TX PDU %u (CAN 0x%lX), len=%u",
          (unsigned int)TxPduId, (unsigned long)cfg->can_id,
          (unsigned int)PduInfoPtr->SduLength);

    return E_OK;
}