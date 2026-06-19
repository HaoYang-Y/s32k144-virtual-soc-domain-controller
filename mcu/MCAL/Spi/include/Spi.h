/**
 * @file    Spi.h
 * @brief   [SKELETON] AUTOSAR MCAL Spi 驱动头文件
 *
 * @note    对应 AUTOSAR CP MCAL Spi 驱动程序规范
 *          基于 S32K144 LPSPI0 硬件
 *          API: Spi_Init, Spi_DeInit, Spi_WriteIB, Spi_ReadIB, Spi_SyncTransmit
 */

#ifndef MCAL_SPI_H
#define MCAL_SPI_H

#include "Std_Types.h"
#include "Spi_Cfg.h"

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief SPI 通道 ID */
typedef uint8_t Spi_ChannelType;

/** @brief SPI 通道结果状态 */
typedef enum {
    SPI_UNINIT = 0,
    SPI_IDLE   = 1,
    SPI_BUSY   = 2
} Spi_StatusType;

/** @brief SPI 序列结果类型 */
typedef enum {
    SPI_JOB_OK              = 0,
    SPI_JOB_PENDING         = 1,
    SPI_JOB_FAILED          = 2,
    SPI_JOB_QUEUED          = 3
} Spi_JobResultType;

/** @brief 数据缓冲类型 */
typedef enum {
    SPI_DATA_LSB_FIRST = 0,
    SPI_DATA_MSB_FIRST = 1
} Spi_DataOrderType;

/** @brief SPI 传输模式 */
typedef enum {
    SPI_MODE_0 = 0,  /* CPOL=0, CPHA=0 */
    SPI_MODE_1 = 1,  /* CPOL=0, CPHA=1 */
    SPI_MODE_2 = 2,  /* CPOL=1, CPHA=0 */
    SPI_MODE_3 = 3   /* CPOL=1, CPHA=1 */
} Spi_ModeType;

/** @brief SPI 通道配置 */
typedef struct {
    Spi_ChannelType     channel_id;
    uint32              baudrate;
    Spi_DataOrderType   data_order;
    Spi_ModeType        spi_mode;
} Spi_ChannelConfigType;

/** @brief SPI 配置容器 */
typedef struct {
    const Spi_ChannelConfigType *channels;
    uint8                        num_channels;
} Spi_ConfigType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

void         Spi_Init(const Spi_ConfigType *ConfigPtr);
void         Spi_DeInit(void);
Spi_StatusType Spi_GetStatus(void);
Spi_JobResultType Spi_GetJobResult(uint8 JobId);
Spi_JobResultType Spi_GetSequenceResult(uint8 SequenceId);

Spi_JobResultType Spi_SyncTransmit(Spi_SequenceType SeqId);
Spi_JobResultType Spi_AsyncTransmit(Spi_SequenceType SeqId);

void         Spi_WriteIB(Spi_ChannelType Channel, const uint8 *Data);
void         Spi_ReadIB(Spi_ChannelType Channel, uint8 *Data);
void         Spi_Exchange(Spi_ChannelType Channel, const uint8 *TxData, uint8 *RxData, uint16 Length);

#endif /* MCAL_SPI_H */
