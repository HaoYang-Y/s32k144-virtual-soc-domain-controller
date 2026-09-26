/**
 * @file    Spi_Cfg.c
 * @brief   SPI 预编译配置实例 (Channel/Job/Sequence 数据)
 *
 * @note    单从机学习场景:
 *            - 1 通道 (IB 全双工: TX/RX 各 64 字节) + 1 作业 + 1 序列
 *            - 硬件: LPSPI1 Slave, PCS3 (PTB17), Mode 0, 8bit, MSB first
 *            - 传输超时 60s (SOC 手动发起, MCU 阻塞等待)
 */

#include "Spi_Cfg.h"

/* ===================================================================
 *  IB 内部缓冲 (驱动持有, ISR/WriteIB 写入)
 * =================================================================== */

/** @brief 通道 0 TX IB (MCU→SOC 方向) */
static Spi_DataBufferType Spi_TxIB[SPI_MAX_DATA_LEN];

/** @brief 通道 0 RX IB (SOC→MCU 方向) */
static Spi_DataBufferType Spi_RxIB[SPI_MAX_DATA_LEN];

/** @brief 通道 0 的 IB 缓冲指针数组 {TX, RX} (全双工 slave 各一个) */
static Spi_DataBufferType * const Spi_IbBufPtr[2U] = { Spi_TxIB, Spi_RxIB };

/* ===================================================================
 *  通道配置
 * =================================================================== */

/** @brief 通道 0: IB 全双工, 8bit, MSB first, Mode 0, PCS3 */
static const Spi_ChannelConfigType Spi_ChannelConfigs[SPI_CHANNEL_COUNT] = {
    {
        .SpiChannelId     = 0U,
        .SpiChannelType   = SPI_IB,
        .SpiDataWidth     = SPI_DATAWIDTH_8BIT,
        .SpiTransferStart = SPI_MSB_FIRST,
        .SpiLength        = SPI_MAX_DATA_LEN,
        .SpiIbNBuffers    = 2U,
        .SpiIbNBufPtr     = Spi_IbBufPtr,
        /* --- 工程扩展: 硬件参数 --- */
        .SpiMode          = SPI_MODE_0,   /* CPOL=0 CPHA=0, 与 CH347T 一致 */
        .WhichPcs         = 3U,           /* PCS3 = PTB17 (CH347T CS0) */
        .DefaultData      = 0xFFU         /* WriteIB(NULL) 时的默认发送字节 */
    }
};

/* ===================================================================
 *  作业配置
 * =================================================================== */

/** @brief 作业 0: 绑定通道 0 */
static const Spi_JobConfigType Spi_JobConfigs[SPI_JOB_COUNT] = {
    {
        .SpiJobId            = 0U,
        .SpiJobPriority      = 0U,   /* 单作业无优先级竞争 */
        .SpiDeviceAssignment = 0U    /* 通道 0 */
    }
};

/* ===================================================================
 *  序列配置
 * =================================================================== */

/** @brief 序列 0 的作业列表 */
static const Spi_JobType Spi_JobList0[1U] = { 0U };

/** @brief 序列 0: 1 作业, 中断机制, 不可中断, 60s 超时 */
static const Spi_SequenceConfigType Spi_SequenceConfigs[SPI_SEQUENCE_COUNT] = {
    {
        .SpiSequenceId            = 0U,
        .SpiTransferMode          = SPI_INTERRUPT_MODE,
        .SpiNumberOfJobs          = 1U,
        .SpiJobList               = Spi_JobList0,
        .SpiInterruptibleSequence = FALSE,
        .TimeoutMs                = 60000UL   /* SOC 手动发起, 等待窗口 60s */
    }
};

/* ===================================================================
 *  Spi_ConfigType 配置容器
 * =================================================================== */

/**
 * @brief   SPI 默认配置实例
 * @note    通知回调暂为 NULL (上层 SpiIf 尚未接入);
 *          后续在 SpiIf 完成时替换为真实通知函数
 */
const Spi_ConfigType Spi_Config = {
    .SpiMaxChannel          = 0U,
    .SpiMaxJob              = 0U,
    .SpiMaxSequence         = 0U,
    .SpiChannelConfigData   = Spi_ChannelConfigs,
    .SpiJobConfigData       = Spi_JobConfigs,
    .SpiSequenceConfigData  = Spi_SequenceConfigs,
    .SpiJobEndNotification      = NULL_PTR,
    .SpiSequenceEndNotification = NULL_PTR
};
