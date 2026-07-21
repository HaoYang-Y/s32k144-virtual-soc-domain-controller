/**
 * @file    CanTp.c
 * @brief   [AUTOSAR CP] CAN Transport Layer 实现 — ISO 15765-2
 *
 * @note    N-PDU 处理核心:
 *          TX: I-PDU → 分段为 N-PDU (SF/FF/CF) → CanIf_Transmit
 *          RX: CanIf 接收 N-PDU (SF/FF/CF) → 重组为 I-PDU → PduR
 *          FC: 流控帧收发
 *
 *          PCI 字节编码规则 (ISO 15765-2):
 *          - SF: [0x0N] [data...]          N = 数据长度 (1-7)
 *          - FF: [0x1N NH] [data 6 bytes]  N NH = 12-bit 总长度 (8-4095)
 *          - CF: [0x2N] [data 7 bytes]     N = 序号 (0-15, 回绕)
 *          - FC: [0x30] [BS] [STmin]       BS = 块大小, STmin = 最小间隔
 */

#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "CanIf.h"
#include "PduR.h"
#include "Log.h"

/* ===================================================================
 *  模块常量
 * =================================================================== */

#define CANTP_MODULE_ID       0x34U       /* CanTp ModuleId */

/* PCI 字节偏移和掩码 */
#define PCI_SF_MASK           0x0FU       /* SF: 低 4 位 = 长度 */
#define PCI_FF_MASK_LOW       0x0FU       /* FF: 低 4 位 + 下一字节 = 12-bit 长度 */
#define PCI_CF_MASK_SEQ       0x0FU       /* CF: 低 4 位 = 序号 */
#define PCI_TYPE_SHIFT        4U          /* 高 4 位 = 帧类型 */

#define PCI_SF_TYPE            0x00U       /* 0000 = SF */
#define PCI_FF_TYPE            0x10U       /* 0001 = FF */
#define PCI_CF_TYPE            0x20U       /* 0010 = CF */
#define PCI_FC_TYPE            0x30U       /* 0011 = FC */

/* 各帧类型数据容量 */
#define SF_DATA_MAX            7U          /* SF payload: 7 bytes */
#define FF_DATA_MAX            6U          /* FF payload: 6 bytes (remaining in CFs) */
#define CF_DATA_MAX            7U          /* CF payload: 7 bytes */

/* FC 参数 */
#define FC_CTS                 0x00U       /* CTS = Continue To Send */
#define FC_WAIT                0x01U       /* WT  = Wait */
#define FC_OVFLW               0x02U       /* OVFLW = Overflow */

/* ===================================================================
 *  通道状态
 * =================================================================== */

typedef struct {
    CanTp_StateType  state;                     /* 当前状态 */
    uint16_t         tp_pdu_id;                 /* 本通道 TP PDU ID */
    uint16_t         total_length;              /* FF 中声明的总长度 */
    uint16_t         rx_index;                  /* RX 已接收字节数 */
    uint8_t          rx_buffer[CANTP_RX_BUFFER_SIZE]; /* RX 重组缓冲区 */
    uint8_t          cf_seq_expected;           /* 期望的下一个 CF 序号 */
    uint8_t          cf_remaining;              /* 当前块剩余 CF 数 */
    uint8_t          cf_sent;                   /* 已发送 CF 数 */
} CanTp_ChannelType;

static CanTp_ChannelType CanTp_Channels[CANTP_CHANNEL_COUNT];
static bool              CanTp_Initialized = false;

/* ===================================================================
 *  内部: 按 TP PDU ID 查找通道
 * =================================================================== */

static CanTp_ChannelType *CanTp_FindChannel(PduIdType PduId)
{
    for (uint8_t i = 0U; i < CANTP_CHANNEL_COUNT; i++) {
        if (CanTp_Channels[i].tp_pdu_id == PduId) {
            return &CanTp_Channels[i];
        }
    }
    return NULL;
}

/* ===================================================================
 *  内部: 按 TP PDU ID 查找 N-PDU 配置
 * =================================================================== */

static const CanTp_NPduConfigType *CanTp_FindConfig(PduIdType PduId)
{
    for (uint8_t i = 0U; i < CanTp_NPduConfig_Count; i++) {
        if (CanTp_NPduConfig[i].tp_pdu_id == PduId) {
            return &CanTp_NPduConfig[i];
        }
    }
    return NULL;
}

/* ===================================================================
 *  内部: 发送单个 N-PDU 到 CanIf
 * =================================================================== */

static Std_ReturnType CanTp_SendNPdu(const CanTp_NPduConfigType *cfg,
                                     const uint8_t *data, uint8_t length)
{
    PduInfoType pduInfo;
    pduInfo.SduId       = cfg->canif_pdu_id;
    pduInfo.SduLength   = length;
    pduInfo.SduDataPtr  = (uint8_t *)data;  /* const cast — CanIf 只读 */

    return CanIf_Transmit(cfg->canif_pdu_id, &pduInfo);
}

/* ===================================================================
 *  PCI 编解码 (公开, 可测试)
 * =================================================================== */

uint8_t CanTp_EncodeSF(uint8_t dataLength)
{
    /* SF PCI: 0x0N, N = data length */
    return (uint8_t)(PCI_SF_TYPE | (dataLength & PCI_SF_MASK));
}

void CanTp_EncodeFF(uint16_t totalLength, uint8_t pci[2])
{
    /* FF PCI: [0x1N][NH], 12-bit total length */
    pci[0] = (uint8_t)(PCI_FF_TYPE | ((totalLength >> 8) & PCI_FF_MASK_LOW));
    pci[1] = (uint8_t)(totalLength & 0xFFU);
}

uint8_t CanTp_EncodeCF(uint8_t seqNum)
{
    /* CF PCI: 0x2N, N = sequence number (0-15) */
    return (uint8_t)(PCI_CF_TYPE | (seqNum & PCI_CF_MASK_SEQ));
}

void CanTp_EncodeFC(uint8_t bs, uint8_t stmin, uint8_t pci[3])
{
    /* FC PCI: [0x30][BS][STmin] */
    pci[0] = PCI_FC_TYPE | FC_CTS;  /* CTS = 0, 即 0x30 */
    pci[1] = bs;
    pci[2] = stmin;
}

CanTp_FrameType CanTp_DecodePci(const uint8_t *data, uint8_t *sfLen,
                                uint16_t *ffTotalLen, uint8_t *cfSeqNum)
{
    uint8_t pci = data[0];
    uint8_t type = (pci >> PCI_TYPE_SHIFT) & 0x0FU;

    switch (type) {
    case 0x0U:  /* SF */
        if (sfLen != NULL) {
            *sfLen = pci & PCI_SF_MASK;
        }
        return CANTP_SF;

    case 0x1U:  /* FF */
        if (ffTotalLen != NULL) {
            *ffTotalLen = (uint16_t)(((uint16_t)(pci & PCI_FF_MASK_LOW) << 8)
                                     | (uint16_t)data[1]);
        }
        return CANTP_FF;

    case 0x2U:  /* CF */
        if (cfSeqNum != NULL) {
            *cfSeqNum = pci & PCI_CF_MASK_SEQ;
        }
        return CANTP_CF;

    case 0x3U:  /* FC */
        return CANTP_FC;

    default:
        return CANTP_SF;  /* fallback */
    }
}

/* ===================================================================
 *  API 实现
 * =================================================================== */

void CanTp_Init(void)
{
    for (uint8_t i = 0U; i < CANTP_CHANNEL_COUNT; i++) {
        CanTp_Channels[i].state    = CANTP_IDLE;
        CanTp_Channels[i].tp_pdu_id = CanTp_NPduConfig[i].tp_pdu_id;
        CanTp_Channels[i].total_length = 0U;
        CanTp_Channels[i].rx_index     = 0U;
        CanTp_Channels[i].cf_seq_expected = 0U;
        CanTp_Channels[i].cf_remaining = 0U;
        CanTp_Channels[i].cf_sent     = 0U;
        for (uint16_t j = 0U; j < CANTP_RX_BUFFER_SIZE; j++) {
            CanTp_Channels[i].rx_buffer[j] = 0U;
        }
    }
    CanTp_Initialized = true;
    LOG_I("CanTp", "Init done, %u channel(s)", (unsigned int)CANTP_CHANNEL_COUNT);
}

Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr)
{
    if (!CanTp_Initialized || PduInfoPtr == NULL) {
        return E_NOT_OK;
    }

    const CanTp_NPduConfigType *cfg = CanTp_FindConfig(TxPduId);
    if (cfg == NULL) {
        LOG_E("CanTp", "Transmit: invalid PduId=%u", (unsigned int)TxPduId);
        return E_NOT_OK;
    }

    CanTp_ChannelType *ch = CanTp_FindChannel(TxPduId);
    if (ch == NULL) {
        return E_NOT_OK;
    }

    uint16_t totalLen = PduInfoPtr->SduLength;
    const uint8_t *srcData = PduInfoPtr->SduDataPtr;
    uint8_t pciBuf[8];  /* N-PDU buffer: PCI + data (max 8 bytes for classic CAN) */

    if (totalLen <= SF_DATA_MAX) {
        /* ================================================================
         *  Single Frame (SF): 单帧 ≤ 7 bytes
         *  CAN 帧布局: [PCI=0x0N][data 0..N-1][padding]
         * ================================================================ */
        pciBuf[0] = CanTp_EncodeSF((uint8_t)totalLen);
        for (uint16_t i = 0U; i < (uint16_t)totalLen; i++) {
            pciBuf[1U + i] = srcData[i];
        }
        /* 填充剩余字节 (可选, 由硬件忽略) */
        for (uint16_t i = (uint16_t)totalLen + 1U; i < 8U; i++) {
            pciBuf[i] = 0x00U;
        }

        LOG_D("CanTp", "TX SF: Pdu=%u, len=%u", (unsigned int)TxPduId,
              (unsigned int)totalLen);

        return CanTp_SendNPdu(cfg, pciBuf, 8U);

    } else {
        /* ================================================================
         *  Multi Frame: FF + CF
         *  FF 布局: [PCI=0x1N NH][data 0..5]
         *  CF 布局: [PCI=0x2N][data 6..12], [PCI=0x2(N+1)][data 13..19]...
         *
         *  简化实现: 连续发送 FF 后立即发送所有 CF (不等待 FC, 用于轮询模式测试)
         *  TODO: 完整 FC 流控状态机
         * ================================================================ */
        uint16_t remaining = totalLen;
        uint16_t srcOffset = 0U;
        uint8_t  seqNum    = 1U;

        /* 发送 FF */
        CanTp_EncodeFF(totalLen, &pciBuf[0]);
        {
            uint8_t ffBytes = (totalLen > FF_DATA_MAX) ? FF_DATA_MAX : (uint8_t)totalLen;
            for (uint8_t i = 0U; i < ffBytes; i++) {
                pciBuf[2U + i] = srcData[i];
            }
            for (uint8_t i = ffBytes; i < FF_DATA_MAX; i++) {
                pciBuf[2U + i] = 0x00U;  /* 填充 */
            }
        }

        LOG_D("CanTp", "TX FF: Pdu=%u, totalLen=%u", (unsigned int)TxPduId,
              (unsigned int)totalLen);

        if (CanTp_SendNPdu(cfg, pciBuf, 8U) != E_OK) {
            LOG_E("CanTp", "TX FF failed: Pdu=%u", (unsigned int)TxPduId);
            return E_NOT_OK;
        }

        srcOffset = FF_DATA_MAX;
        remaining = totalLen - FF_DATA_MAX;

        /* 发送 CFs (简化: 无 FC 等待, 顺序发送) */
        while (remaining > 0U) {
            uint8_t cfLen = (remaining > CF_DATA_MAX) ? CF_DATA_MAX : (uint8_t)remaining;
            pciBuf[0] = CanTp_EncodeCF(seqNum);
            for (uint8_t i = 0U; i < cfLen; i++) {
                pciBuf[1U + i] = srcData[srcOffset + i];
            }
            for (uint8_t i = cfLen; i < CF_DATA_MAX; i++) {
                pciBuf[1U + i] = 0x00U;
            }

            if (CanTp_SendNPdu(cfg, pciBuf, 8U) != E_OK) {
                LOG_E("CanTp", "TX CF[%u] failed: Pdu=%u", (unsigned int)seqNum,
                      (unsigned int)TxPduId);
                return E_NOT_OK;
            }

            LOG_D("CanTp", "TX CF[%u]: Pdu=%u, cfLen=%u", (unsigned int)seqNum,
                  (unsigned int)TxPduId, (unsigned int)cfLen);

            srcOffset += cfLen;
            remaining  = (remaining > cfLen) ? (uint16_t)(remaining - cfLen) : 0U;
            seqNum     = (seqNum + 1U) & 0x0FU;
        }

        ch->state = CANTP_IDLE;
        return E_OK;
    }
}

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    if (!CanTp_Initialized || PduInfoPtr == NULL) {
        return;
    }
    if (PduInfoPtr->SduLength == 0U) {
        return;
    }

    /* 查找通道 (RX: 需要匹配 canif_pdu_id → tp_pdu_id) */
    CanTp_ChannelType *ch = NULL;
    const CanTp_NPduConfigType *cfg = NULL;
    for (uint8_t i = 0U; i < CANTP_CHANNEL_COUNT; i++) {
        if (CanTp_NPduConfig[i].canif_pdu_id == (uint16_t)RxPduId) {
            ch  = &CanTp_Channels[i];
            cfg = &CanTp_NPduConfig[i];
            break;
        }
    }
    if (ch == NULL || cfg == NULL) {
        LOG_E("CanTp", "RxIndication: no channel for PduId=%u",
              (unsigned int)RxPduId);
        return;
    }

    const uint8_t *data   = PduInfoPtr->SduDataPtr;
    uint8_t        sfLen  = 0U;
    uint16_t       ffTotalLen = 0U;
    uint8_t        cfSeqNum   = 0U;
    CanTp_FrameType frameType = CanTp_DecodePci(data, &sfLen, &ffTotalLen, &cfSeqNum);

    switch (frameType) {
    case CANTP_SF:
        /* ================================================================
         *  Single Frame: 直接重组为 I-PDU → PduR
         * ================================================================ */
        {
            PduInfoType iPDU;
            iPDU.SduId      = cfg->tp_pdu_id;
            iPDU.SduLength  = sfLen;
            iPDU.SduDataPtr = (uint8_t *)&data[1];  /* 跳过 PCI 字节 */

            LOG_D("CanTp", "RX SF: Pdu=%u, len=%u", (unsigned int)cfg->tp_pdu_id,
                  (unsigned int)sfLen);

            PduR_CanIfRxIndication(cfg->tp_pdu_id, &iPDU);
        }
        break;

    case CANTP_FF:
        /* ================================================================
         *  First Frame: 提取总长度 + 前 6 字节数据 → 发送 FC → 等待 CF
         * ================================================================ */
        {
            ch->total_length     = ffTotalLen;
            ch->rx_index         = 0U;
            ch->cf_seq_expected  = 1U;
            ch->state            = CANTP_RECEIVING;

            /* 复制 FF 中的前 6 字节数据 */
            {
                uint8_t ffDataLen = (ffTotalLen > FF_DATA_MAX) ? FF_DATA_MAX
                                                                : (uint8_t)ffTotalLen;
                for (uint8_t i = 0U; i < ffDataLen; i++) {
                    ch->rx_buffer[ch->rx_index++] = data[2U + i];
                }
            }

            LOG_D("CanTp", "RX FF: Pdu=%u, totalLen=%u",
                  (unsigned int)cfg->tp_pdu_id, (unsigned int)ffTotalLen);

            /* 发送 FC (Flow Control) — CTS, BS=8, STmin=100us */
            {
                uint8_t fcPci[8];
                CanTp_EncodeFC(CANTP_BLOCK_SIZE,
                               (uint8_t)(CANTP_STMIN_US / 100U), &fcPci[0]);
                fcPci[3] = 0xAAU; /* padding */
                fcPci[4] = 0xAAU;
                fcPci[5] = 0xAAU;
                fcPci[6] = 0xAAU;
                fcPci[7] = 0xAAU;

                (void)CanTp_SendNPdu(cfg, fcPci, 8U);
                LOG_D("CanTp", "TX FC: Pdu=%u, BS=%u, STmin=%u",
                      (unsigned int)cfg->tp_pdu_id,
                      (unsigned int)CANTP_BLOCK_SIZE,
                      (unsigned int)CANTP_STMIN_US);
            }
        }
        break;

    case CANTP_CF:
        /* ================================================================
         *  Consecutive Frame: 累积数据 → 收完提交 I-PDU
         * ================================================================ */
        if (ch->state != CANTP_RECEIVING) {
            LOG_E("CanTp", "RX unexpected CF: Pdu=%u (state=%d)",
                  (unsigned int)cfg->tp_pdu_id, (int)ch->state);
            return;
        }
        {
            uint16_t remaining = ch->total_length - ch->rx_index;
            uint8_t  cfDataLen = (remaining > CF_DATA_MAX) ? CF_DATA_MAX
                                                           : (uint8_t)remaining;

            /* 复制 CF 数据 (跳过 PCI 字节) */
            for (uint8_t i = 0U; i < cfDataLen; i++) {
                if (ch->rx_index < CANTP_RX_BUFFER_SIZE) {
                    ch->rx_buffer[ch->rx_index++] = data[1U + i];
                }
            }

            LOG_D("CanTp", "RX CF[%u]: Pdu=%u, rx=%u/%u", (unsigned int)cfSeqNum,
                  (unsigned int)cfg->tp_pdu_id,
                  (unsigned int)ch->rx_index, (unsigned int)ch->total_length);

            ch->cf_seq_expected = (cfSeqNum + 1U) & 0x0FU;

            /* 检查是否接收完成 */
            if (ch->rx_index >= ch->total_length) {
                PduInfoType iPDU;
                iPDU.SduId      = cfg->tp_pdu_id;
                iPDU.SduLength  = ch->total_length;
                iPDU.SduDataPtr = ch->rx_buffer;

                LOG_D("CanTp", "RX complete: Pdu=%u, len=%u",
                      (unsigned int)cfg->tp_pdu_id, (unsigned int)ch->total_length);

                PduR_CanIfRxIndication(cfg->tp_pdu_id, &iPDU);

                ch->state        = CANTP_IDLE;
                ch->total_length = 0U;
                ch->rx_index     = 0U;
            }
        }
        break;

    case CANTP_FC:
        /* ================================================================
         *  Flow Control (TX 侧)
         *  TODO: 完整的 FC → CF 发送状态机
         * ================================================================ */
        LOG_D("CanTp", "RX FC: Pdu=%u (ignored in simplified mode)",
              (unsigned int)cfg->tp_pdu_id);
        break;

    default:
        break;
    }
}

void CanTp_TxConfirmation(PduIdType TxPduId)
{
    if (!CanTp_Initialized) return;

    LOG_D("CanTp", "TX confirm Pdu=%u", (unsigned int)TxPduId);

    /* TODO: 通知 PduR_CanIfTxConfirmation(TxPduId) */
    (void)TxPduId;
}