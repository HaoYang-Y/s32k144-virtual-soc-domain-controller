/**
 * @file    Spi.c
 * @brief   AUTOSAR MCAL Spi 驱动实现 — LPSPI1 Slave 模式
 *
 * @note    基于 S32K144 LPSPI1 + SDK lpspi_slave_driver
 *          Master (SOC/CH347T) 发起传输，Slave (MCU) 响应
 *
 *          Slave 引脚 (ALT3):
 *            PTB14 = LPSPI1_SCK
 *            PTB16 = LPSPI1_SIN  ← CH347T MO
 *            PTB15 = LPSPI1_SOUT → CH347T MI
 *            PTB17 = LPSPI1_PCS3 ← CH347T CS0
 */

#include "Spi.h"
#include "lpspi_slave_driver.h"
#include "lpspi_hw_access.h"
#include "interrupt_manager.h"
#include <string.h>

/* LPSPI1 实例索引 (SDK slave driver 使用) */
#define LPSPI1_INSTANCE  1U

/* 收发缓冲区大小 (与 SOC 端 spidev_test 匹配) */
#define SPI_SLAVE_BUF_SIZE  64U

/* ===================================================================
 *  模块全局变量
 * =================================================================== */

static const Spi_ConfigType *Spi_ConfigPtr = NULL_PTR;
static Spi_StatusType        Spi_Status    = SPI_UNINIT;

/** LPSPI1 Slave 状态句柄 (SDK 要求全局可访问) */
static lpspi_state_t         Spi_SlaveState;

/** 收发缓冲区: TX 预填 echo 数据, RX 接收 Master 发来的数据 */
static uint8_t               Spi_TxBuf[SPI_SLAVE_BUF_SIZE];
static uint8_t               Spi_RxBuf[SPI_SLAVE_BUF_SIZE];

/** 是否已至少完成一次传输 */
static volatile bool         Spi_SlaveRxDone = false;

/* ===================================================================
 *  API 实现
 * =================================================================== */

void Spi_Init(const Spi_ConfigType *ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        return;
    }
    Spi_ConfigPtr = ConfigPtr;
    Spi_Status    = SPI_IDLE;
}

void Spi_DeInit(void)
{
    (void)LPSPI_DRV_SlaveDeinit(LPSPI1_INSTANCE);
    Spi_Status    = SPI_UNINIT;
    Spi_ConfigPtr = NULL_PTR;
}

Spi_StatusType Spi_GetStatus(void)
{
    return Spi_Status;
}

Spi_JobResultType Spi_GetJobResult(uint8 JobId)
{
    (void)JobId;
    return SPI_JOB_OK;
}

Spi_JobResultType Spi_GetSequenceResult(uint8 SequenceId)
{
    (void)SequenceId;
    return SPI_JOB_OK;
}

Spi_JobResultType Spi_SyncTransmit(Spi_SequenceType SeqId)
{
    (void)SeqId;
    return SPI_JOB_OK;
}

Spi_JobResultType Spi_AsyncTransmit(Spi_SequenceType SeqId)
{
    (void)SeqId;
    return SPI_JOB_OK;
}

void Spi_WriteIB(Spi_ChannelType Channel, const uint8 *Data)
{
    (void)Channel;
    (void)Data;
}

void Spi_ReadIB(Spi_ChannelType Channel, uint8 *Data)
{
    (void)Channel;
    (void)Data;
}

void Spi_Exchange(Spi_ChannelType Channel, const uint8 *TxData,
                  uint8 *RxData, uint16 Length)
{
    (void)Channel;
    (void)TxData;
    (void)RxData;
    (void)Length;
}

/* ===================================================================
 *  Slave 模式实现
 * =================================================================== */

/**
 * @brief   初始化 LPSPI1 为 Slave 模式 (PCS3 片选, 中断驱动)
 * @param   pcs_index  PCS 引脚 (0=PCS0, 2=PCS2, 3=PCS3)
 * @return  STATUS_SUCCESS / STATUS_ERROR
 */
uint32_t Spi_SlaveInit(uint8_t pcs_index)
{
    lpspi_slave_config_t cfg;
    status_t             st;

    /* 获取默认 Slave 配置 */
    LPSPI_DRV_SlaveGetDefaultConfig(&cfg);

    /* 覆盖: PCS3 片选, CPOL=0 CPHA=0 (SPI_MODE_0, 与 CH347T 匹配) */
    cfg.whichPcs     = (pcs_index == 0U) ? LPSPI_PCS0 :
                       (pcs_index == 2U) ? LPSPI_PCS2 :
                       (pcs_index == 3U) ? LPSPI_PCS3 : LPSPI_PCS3;
    cfg.clkPolarity  = LPSPI_SCK_ACTIVE_HIGH;  /* CPOL=0: SCK idles low */
    cfg.clkPhase     = LPSPI_CLOCK_PHASE_1ST_EDGE;  /* CPHA=0: capture on 1st edge */
    cfg.pcsPolarity  = LPSPI_ACTIVE_LOW;       /* PCS active low */
    cfg.bitcount     = 8U;
    cfg.lsbFirst     = false;
    cfg.transferType = LPSPI_USING_INTERRUPTS;

    /* 初始化 TX buffer 为递增模式 (方便验证) */
    for (uint8_t i = 0U; i < SPI_SLAVE_BUF_SIZE; i++) {
        Spi_TxBuf[i] = 0xA0U + i;  /* [A0, A1, A2, ..., DF] */
    }
    (void)memset(Spi_RxBuf, 0, sizeof(Spi_RxBuf));

    st = LPSPI_DRV_SlaveInit(LPSPI1_INSTANCE, &Spi_SlaveState, &cfg);
    if (st != STATUS_SUCCESS) {
        return (uint32_t)STATUS_ERROR;
    }

    Spi_SlaveRxDone = false;
    return (uint32_t)STATUS_SUCCESS;
}

/**
 * @brief   Slave 阻塞收发 — 纯轮询模式, 不依赖 SDK ISR
 * @param   tx_data      发送缓冲区 (NULL = 使用预填模式)
 * @param   rx_data      接收缓冲区 (NULL = 仅丢弃)
 * @param   byte_count   期望传输字节数 (≤ SPI_SLAVE_BUF_SIZE)
 * @param   timeout_ms   超时时间 (ms)
 * @return  0=成功, 非0=超时
 */
uint32_t Spi_SlaveExchange(const uint8_t *tx_data, uint8_t *rx_data,
                           uint16_t byte_count, uint32_t timeout_ms)
{
    uint16_t tx_idx = 0U;
    uint16_t rx_idx = 0U;

    if (byte_count > SPI_SLAVE_BUF_SIZE) {
        byte_count = SPI_SLAVE_BUF_SIZE;
    }

    /* 准备 TX 数据 */
    if (tx_data != NULL) {
        (void)memcpy(Spi_TxBuf, tx_data, byte_count);
    }

    /* 清除状态标志 */
    LPSPI1->SR = LPSPI_SR_RDF_MASK | LPSPI_SR_TDF_MASK
               | LPSPI_SR_WCF_MASK | LPSPI_SR_FCF_MASK
               | LPSPI_SR_TCF_MASK | LPSPI_SR_TEF_MASK
               | LPSPI_SR_REF_MASK | LPSPI_SR_DMF_MASK;

    /* 预填 TX FIFO (最多 4 word = FIFO 深度) */
    {
        uint16_t n = (byte_count < 4U) ? byte_count : 4U;
        for (uint16_t i = 0U; i < n; i++) {
            LPSPI_WriteData(LPSPI1, (uint32_t)Spi_TxBuf[i]);
        }
        tx_idx = n;
    }

    /* 忙等轮询 RX 数据 + 补填 TX FIFO (纯轮询, 不用 ISR) */
    {
        volatile uint32_t poll_cnt = timeout_ms * 24000U;
        while (rx_idx < byte_count && poll_cnt > 0U) {
            uint32_t sr = LPSPI1->SR;

            /* RX: 读到数据就取走 */
            if ((sr & LPSPI_SR_RDF_MASK) != 0U) {
                LPSPI1->SR = LPSPI_SR_RDF_MASK;
                while ((LPSPI1->SR & LPSPI_SR_RDF_MASK) != 0U && rx_idx < SPI_SLAVE_BUF_SIZE) {
                    Spi_RxBuf[rx_idx++] = (uint8_t)LPSPI_ReadData(LPSPI1);
                }
            }

            /* TX: FIFO 有空位就填 */
            if ((sr & LPSPI_SR_TDF_MASK) != 0U && tx_idx < byte_count) {
                LPSPI1->SR = LPSPI_SR_TDF_MASK;
                while ((LPSPI1->SR & LPSPI_SR_TDF_MASK) != 0U && tx_idx < byte_count) {
                    LPSPI_WriteData(LPSPI1, (uint32_t)Spi_TxBuf[tx_idx++]);
                }
            }

            /* 错误处理 */
            if ((sr & (LPSPI_SR_TEF_MASK | LPSPI_SR_REF_MASK)) != 0U) {
                LPSPI1->SR = LPSPI_SR_TEF_MASK | LPSPI_SR_REF_MASK;
            }

            poll_cnt--;
            __asm__("nop");
        }

        if (rx_idx < byte_count) {
            return 1U;  /* 超时 */
        }
    }

    /* 复制 RX 数据到用户缓冲 */
    if (rx_data != NULL) {
        (void)memcpy(rx_data, Spi_RxBuf, byte_count);
    }

    /* 准备 echo: 将收到的数据作为下次 TX */
    (void)memcpy(Spi_TxBuf, Spi_RxBuf, byte_count);

    Spi_SlaveRxDone = true;
    return 0U;
}

/**
 * @brief   查询是否已收到过至少一次 SPI 传输
 * @return  true: 已收到数据, false: 尚未收到
 */
bool Spi_SlaveHasReceived(void)
{
    return Spi_SlaveRxDone;
}

/* ===================================================================
 *  SDK 依赖桩函数 (Slave-only 模式下不需要 Master 功能)
 * =================================================================== */

/**
 * @brief   LPSPI Master IRQ Handler 桩 — Slave 模式不需要 Master
 *
 * @note    lpspi_shared_function.c 的 LPSPI_DRV_IRQHandler 会调用此函数,
 *          但我们只使用 Slave 模式, Master 中断不会触发, 提供空桩即可
 */
void LPSPI_DRV_MasterIRQHandler(uint32_t instance)
{
    (void)instance;
}
