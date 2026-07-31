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

    /* === RX 上下文 (多帧重组) === */
    uint16_t         tp_pdu_id;                 /* 本通道 TP PDU ID */
    uint16_t         total_length;              /* FF 中声明的总长度 */
    uint16_t         rx_index;                  /* RX 已接收字节数 */
    uint8_t          rx_buffer[CANTP_RX_BUFFER_SIZE]; /* RX 重组缓冲区 */
    uint8_t          cf_seq_expected;           /* 期望的下一个 CF 序号 */
    uint8_t          cf_remaining;              /* 当前块剩余 CF 数 */
    uint8_t          cf_sent;                   /* 已发送 CF 数 */

    /* === TX 上下文 (多帧发送 + FC 流控) === */
    uint8_t          tx_buffer[CANTP_RX_BUFFER_SIZE]; /* TX 数据副本 (避免调用方缓冲区生命周期问题) */
    uint16_t         tx_total_length;           /* 待发送总字节数 */
    uint16_t         tx_offset;                 /* 当前发送偏移 (已发送字节数) */
    uint8_t          tx_seq_num;                /* 下一个 CF 序号 (1..15, 回绕) */
    uint8_t          tx_fc_bs;                  /* Block Size (从 FC 帧提取) */
    uint8_t          tx_fc_stmin_ms;            /* STmin 转换为 ms */
    uint8_t          tx_block_cf_sent;          /* 当前块内已发送 CF 数 */
    bool             tx_sf_pending;             /* SF 已发送, 等待 CanIf 确认 (N_As 超时) */
    uint32_t         tx_state_enter_ms;         /* WAIT_FC 或 SF 等待确认的时刻 (N_Bs/N_As 超时) */
    uint32_t         tx_last_cf_ms;             /* 上次发送 CF 的时刻 (STmin 间隔) */
    uint32_t         rx_state_enter_ms;         /* 进入 RECEIVING 状态时的时刻 (N_Cr 超时) */
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
        CanTp_Channels[i].state         = CANTP_IDLE;
        CanTp_Channels[i].tp_pdu_id     = CanTp_NPduConfig[i].tp_pdu_id;

        /* RX 字段 */
        CanTp_Channels[i].total_length     = 0U;
        CanTp_Channels[i].rx_index         = 0U;
        CanTp_Channels[i].cf_seq_expected  = 0U;
        CanTp_Channels[i].cf_remaining     = 0U;
        CanTp_Channels[i].cf_sent          = 0U;

        /* TX 字段 */
        CanTp_Channels[i].tx_total_length  = 0U;
        CanTp_Channels[i].tx_offset        = 0U;
        CanTp_Channels[i].tx_seq_num       = 1U;
        CanTp_Channels[i].tx_fc_bs         = 0U;
        CanTp_Channels[i].tx_fc_stmin_ms   = 0U;
        CanTp_Channels[i].tx_block_cf_sent = 0U;
        CanTp_Channels[i].tx_sf_pending    = false;
        CanTp_Channels[i].tx_state_enter_ms = 0U;
        CanTp_Channels[i].tx_last_cf_ms    = 0U;
        CanTp_Channels[i].rx_state_enter_ms = 0U;

        for (uint16_t j = 0U; j < CANTP_RX_BUFFER_SIZE; j++) {
            CanTp_Channels[i].rx_buffer[j] = 0U;
            CanTp_Channels[i].tx_buffer[j] = 0U;
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

        /* 发送成功 (已入硬件队列) → 置 SF 待确认标志, N_As 超时起点 */
        if (CanTp_SendNPdu(cfg, pciBuf, 8U) == E_OK) {
            ch->tx_sf_pending    = true;
            ch->tx_state_enter_ms = Log_GetTimeMs();
            return E_OK;
        }
        return E_NOT_OK;

    } else {
        /* ================================================================
         *  Multi Frame (MF): FF → 等待 FC → CF (由 MainFunction 驱动)
         *
         *  FF 布局: [PCI=0x1N NH][data 0..5]
         *  CF 布局: [PCI=0x2N][data 6..12], [PCI=0x2(N+1)][data 13..19]...
         *
         *  状态机:
         *    IDLE → (发送 FF) → WAIT_FC → (收到 FC(CTS)) → SENDING_CF
         *    → (发完一块) → WAIT_FC → (收到 FC) → SENDING_CF → ... → IDLE
         * ================================================================ */

        /* 1. 拷贝 I-PDU 到通道 TX 缓冲区 (避免调用方缓冲区生命周期问题) */
        if (totalLen > CANTP_RX_BUFFER_SIZE) {
            LOG_E("CanTp", "TX MF: data too large (%u > %u)",
                  (unsigned int)totalLen, (unsigned int)CANTP_RX_BUFFER_SIZE);
            return E_NOT_OK;
        }
        for (uint16_t i = 0U; i < totalLen; i++) {
            ch->tx_buffer[i] = srcData[i];
        }
        ch->tx_total_length = totalLen;
        ch->tx_offset       = 0U;
        ch->tx_seq_num      = 1U;

        /* 2. 构建并发送 FF (PCI + 前 6 字节数据) */
        CanTp_EncodeFF(totalLen, &pciBuf[0]);
        {
            uint8_t ffBytes = (totalLen > FF_DATA_MAX) ? FF_DATA_MAX : (uint8_t)totalLen;
            for (uint8_t i = 0U; i < ffBytes; i++) {
                pciBuf[2U + i] = srcData[i];
            }
            for (uint8_t i = ffBytes; i < FF_DATA_MAX; i++) {
                pciBuf[2U + i] = 0x00U;
            }
        }

        LOG_I("CanTp", "TX FF: Pdu=%u, totalLen=%u → waiting for FC",
              (unsigned int)TxPduId, (unsigned int)totalLen);

        if (CanTp_SendNPdu(cfg, pciBuf, 8U) != E_OK) {
            LOG_E("CanTp", "TX FF failed: Pdu=%u", (unsigned int)TxPduId);
            ch->state = CANTP_IDLE;
            return E_NOT_OK;
        }

        /* 3. 进入 WAIT_FC 状态 — 后续 CF 由 CanTp_MainFunction 按 FC 指示发送 */
        ch->tx_offset          = FF_DATA_MAX;  /* FF 已携带前 6 字节 */
        ch->state              = CANTP_WAIT_FC;
        ch->tx_state_enter_ms  = Log_GetTimeMs();
        ch->tx_block_cf_sent   = 0U;

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

            PduR_CanTpRxIndication(cfg->tp_pdu_id, &iPDU);
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
            ch->rx_state_enter_ms = Log_GetTimeMs();  /* N_Cr 超时计时起点 */

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

                PduR_CanTpRxIndication(cfg->tp_pdu_id, &iPDU);

                ch->state        = CANTP_IDLE;
                ch->total_length = 0U;
                ch->rx_index     = 0U;
            }
        }
        break;

    case CANTP_FC:
        /* ================================================================
         *  Flow Control (TX 侧)
         *
         *  FC 帧到达 RX 通道，但控制的是 TX 通道的 CF 发送节奏。
         *  策略: 遍历所有通道，找到状态为 CANTP_WAIT_FC 的通道进行控制。
         *
         *  FC PCI 三字节: [0x3F][BS][STmin]
         *    - F = Flow Status: 0=CTS, 1=WAIT, 2=OVFLW
         *    - BS = Block Size (连续发送 CF 数量, 0=无限制)
         *    - STmin = 最小间隔 (单位: 100us, 0=无限制)
         * ================================================================ */
        {
            uint8_t  fc_flow_status = data[0] & PCI_CF_MASK_SEQ;  /* 低 4 位 = Flow Status */
            uint8_t  fc_bs          = data[1];                     /* Block Size */
            uint8_t  fc_stmin_raw   = data[2];                     /* STmin (100us 单位) */

            /* 查找等待 FC 的 TX 通道 */
            CanTp_ChannelType *tx_ch = NULL;
            for (uint8_t k = 0U; k < CANTP_CHANNEL_COUNT; k++) {
                if (CanTp_Channels[k].state == CANTP_WAIT_FC) {
                    tx_ch = &CanTp_Channels[k];
                    break;
                }
            }

            if (tx_ch == NULL) {
                LOG_W("CanTp", "RX FC but no channel waiting: Pdu=%u",
                      (unsigned int)cfg->tp_pdu_id);
                break;
            }

            if (fc_flow_status == FC_CTS) {
                /* ---- CTS: 开始/继续发送 CF ---- */
                tx_ch->tx_fc_bs = fc_bs;

                /* STmin 转换为 ms: STmin * 100us → ms (向上取整，最小 1ms)
                 * 这是简化的时间精度 — ARM DWT 计数器分辨率为 1ms */
                if (fc_stmin_raw == 0U) {
                    tx_ch->tx_fc_stmin_ms = 0U;  /* 0 = 无间隔限制 */
                } else {
                    uint8_t stmin_ms = (uint8_t)((fc_stmin_raw * 100U + 999U) / 1000U);
                    tx_ch->tx_fc_stmin_ms = (stmin_ms > 0U) ? stmin_ms : 1U;
                }

                tx_ch->tx_block_cf_sent = 0U;
                tx_ch->tx_last_cf_ms    = Log_GetTimeMs();
                tx_ch->state            = CANTP_SENDING_CF;

                LOG_I("CanTp", "TX got FC(CTS): Pdu=%u, BS=%u, STmin=%u ms",
                      (unsigned int)tx_ch->tp_pdu_id,
                      (unsigned int)fc_bs,
                      (unsigned int)tx_ch->tx_fc_stmin_ms);

            } else if (fc_flow_status == FC_WAIT) {
                /* ---- WAIT: 重置超时，继续等待下一个 FC ---- */
                tx_ch->tx_state_enter_ms = Log_GetTimeMs();
                LOG_W("CanTp", "TX got FC(WAIT): Pdu=%u, staying in WAIT_FC",
                      (unsigned int)tx_ch->tp_pdu_id);

            } else {
                /* ---- OVFLW (+ 未知值): 中止传输 ---- */
                LOG_E("CanTp", "TX got FC(OVFLW=0x%02X): Pdu=%u, aborting",
                      (unsigned int)fc_flow_status,
                      (unsigned int)tx_ch->tp_pdu_id);
                tx_ch->state          = CANTP_IDLE;
                tx_ch->tx_total_length = 0U;
            }
        }
        break;

    default:
        break;
    }
}

/**
 * @brief CanTp 周期处理函数 (AUTOSAR MainFunction 模式)
 *
 * @note  由 EcuM_MainFunction 从 main 循环周期性调用，负责:
 *        1. CANTP_WAIT_FC:   检查 N_Bs 超时
 *        2. CANTP_SENDING_CF: 按 STmin 间隔 + BS 块大小发送 CF
 *        3. CANTP_RECEIVING:  检查 N_Cr 超时
 *
 *        无 OS/中断辅助，完全由 MainFunction 轮询驱动。
 */
void CanTp_MainFunction(void)
{
    if (!CanTp_Initialized) return;

    uint32_t now = Log_GetTimeMs();

    for (uint8_t i = 0U; i < CANTP_CHANNEL_COUNT; i++) {
        CanTp_ChannelType *ch = &CanTp_Channels[i];

        switch (ch->state) {

        /* ================================================================
         *  IDLE: SF 已发出, 等待 CanIf 确认
         *  超时 (N_As) → 回 IDLE + 报错 (帧仍在发送或已丢失)
         * ================================================================ */
        case CANTP_IDLE:
            if (ch->tx_sf_pending) {
                if ((now - ch->tx_state_enter_ms) >= CANTP_AS_TIMEOUT_MS) {
                    LOG_E("CanTp", "TX N_As timeout: Pdu=%u (no confirm in %u ms)",
                          (unsigned int)ch->tp_pdu_id,
                          (unsigned int)CANTP_AS_TIMEOUT_MS);
                    ch->tx_sf_pending = false;
                }
            }
            break;

        /* ================================================================
         *  WAIT_FC: 等待对端回复 Flow Control
         *  超时 (N_Bs) → 回 IDLE + 报错
         * ================================================================ */
        case CANTP_WAIT_FC:
            if ((now - ch->tx_state_enter_ms) >= CANTP_BS_TIMEOUT_MS) {
                LOG_E("CanTp", "TX N_Bs timeout: Pdu=%u (no FC received in %u ms)",
                      (unsigned int)ch->tp_pdu_id,
                      (unsigned int)CANTP_BS_TIMEOUT_MS);
                ch->state            = CANTP_IDLE;
                ch->tx_total_length  = 0U;
            }
            break;

        /* ================================================================
         *  SENDING_CF: 按节奏发送 Consecutive Frame
         *  - STmin:   两次 CF 之间的最小间隔
         *  - BS:      每个 FC 块允许发送的 CF 数量 (0=无限)
         *  - tx_offset ≥ tx_total_length → 全部发完 → IDLE
         * ================================================================ */
        case CANTP_SENDING_CF:
            /* STmin 间隔检查 */
            if (ch->tx_fc_stmin_ms > 0U) {
                if ((now - ch->tx_last_cf_ms) < ch->tx_fc_stmin_ms) {
                    break;  /* 还没到下次发送时间 */
                }
            }

            /* 块大小检查: 发完 BS 个 CF 后回 WAIT_FC 等下一个 FC */
            if ((ch->tx_fc_bs > 0U) && (ch->tx_block_cf_sent >= ch->tx_fc_bs)) {
                ch->state              = CANTP_WAIT_FC;
                ch->tx_state_enter_ms  = now;
                ch->tx_block_cf_sent   = 0U;
                LOG_D("CanTp", "TX block done (BS=%u), waiting for next FC",
                      (unsigned int)ch->tx_fc_bs);
                break;
            }

            /* 全部发送完成? */
            if (ch->tx_offset >= ch->tx_total_length) {
                LOG_I("CanTp", "TX complete: Pdu=%u, len=%u",
                      (unsigned int)ch->tp_pdu_id,
                      (unsigned int)ch->tx_total_length);
                ch->state = CANTP_IDLE;
                /* I-PDU 发送完成 → 向上确认 (简化: 不等最后一帧 CF 的硬件确认,
                 * 由 CAN_Send 返回值兜底; 严格实现见 TODO) */
                PduR_CanTpTxConfirmation(ch->tp_pdu_id);
                break;
            }

            /* 构建并发送下一帧 CF */
            {
                uint16_t remaining  = ch->tx_total_length - ch->tx_offset;
                uint8_t  cfDataLen  = (remaining > CF_DATA_MAX) ? CF_DATA_MAX
                                                                : (uint8_t)remaining;
                uint8_t  pciBuf[8];

                pciBuf[0] = CanTp_EncodeCF(ch->tx_seq_num);
                for (uint8_t j = 0U; j < cfDataLen; j++) {
                    pciBuf[1U + j] = ch->tx_buffer[ch->tx_offset + j];
                }
                for (uint8_t j = cfDataLen; j < CF_DATA_MAX; j++) {
                    pciBuf[1U + j] = 0x00U;
                }

                const CanTp_NPduConfigType *cfg = CanTp_FindConfig(ch->tp_pdu_id);
                if ((cfg != NULL) && (CanTp_SendNPdu(cfg, pciBuf, 8U) == E_OK)) {
                    ch->tx_offset       = (uint16_t)(ch->tx_offset + cfDataLen);
                    ch->tx_seq_num      = (ch->tx_seq_num + 1U) & 0x0FU;
                    ch->tx_block_cf_sent++;
                    ch->tx_last_cf_ms   = now;

                    LOG_D("CanTp", "TX CF[%u]: Pdu=%u, offset=%u/%u",
                          (unsigned int)((ch->tx_seq_num > 0U) ? ch->tx_seq_num - 1U : 15U),
                          (unsigned int)ch->tp_pdu_id,
                          (unsigned int)ch->tx_offset,
                          (unsigned int)ch->tx_total_length);
                } else {
                    LOG_E("CanTp", "TX CF send failed: Pdu=%u", (unsigned int)ch->tp_pdu_id);
                    ch->state = CANTP_IDLE;
                }
            }
            break;

        /* ================================================================
         *  RECEIVING: 等待 CF 到达完成重组
         *  超时 (N_Cr) → 回 IDLE + 丢弃已接收数据
         * ================================================================ */
        case CANTP_RECEIVING:
            if ((now - ch->rx_state_enter_ms) >= CANTP_CR_TIMEOUT_MS) {
                LOG_E("CanTp", "RX N_Cr timeout: Pdu=%u, only %u/%u bytes received",
                      (unsigned int)ch->tp_pdu_id,
                      (unsigned int)ch->rx_index,
                      (unsigned int)ch->total_length);
                ch->state        = CANTP_IDLE;
                ch->total_length = 0U;
                ch->rx_index     = 0U;
            }
            break;

        default:
            break;
        }
    }
}

void CanTp_TxConfirmation(PduIdType TxPduId)
{
    if (!CanTp_Initialized) return;

    /* 按 CanIf PDU ID 查找通道 (与 RxIndication 一致) */
    CanTp_ChannelType *ch = NULL;
    const CanTp_NPduConfigType *cfg = NULL;
    for (uint8_t i = 0U; i < CANTP_CHANNEL_COUNT; i++) {
        if (CanTp_NPduConfig[i].canif_pdu_id == (uint16_t)TxPduId) {
            ch  = &CanTp_Channels[i];
            cfg = &CanTp_NPduConfig[i];
            break;
        }
    }
    if (ch == NULL || cfg == NULL) {
        LOG_W("CanTp", "TxConfirmation: no channel for PduId=%u",
              (unsigned int)TxPduId);
        return;
    }

    LOG_D("CanTp", "TX confirm Pdu=%u", (unsigned int)TxPduId);

    /* SF: 单帧确认 = 整个 I-PDU 发送完成 → 向上确认 */
    if (ch->tx_sf_pending) {
        ch->tx_sf_pending = false;
        PduR_CanTpTxConfirmation(cfg->tp_pdu_id);
        return;
    }

    /* MF: 中间 CF 的确认由 MainFunction 乐观推进, 此处忽略 */
}