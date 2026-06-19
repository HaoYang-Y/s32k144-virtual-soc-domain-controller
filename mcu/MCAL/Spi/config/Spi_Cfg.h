/**
 * @file    Spi_Cfg.h
 * @brief   [SKELETON] SPI 预编译配置
 *
 * @note    定义 SPI 通道、作业、序列的编译期常量
 */

#ifndef SPI_CFG_H
#define SPI_CFG_H

#include "Std_Types.h"

/** @brief 硬件 SPI 外设索引 */
#define SPI_INSTANCE_ID             0U  /* LPSPI0 */

/** @brief 通道数 */
#define SPI_CHANNEL_COUNT           1U

/** @brief 作业数 */
#define SPI_JOB_COUNT               1U

/** @brief 序列数 */
#define SPI_SEQUENCE_COUNT          1U

/** @brief SPI 通道 ID 枚举 */
typedef uint8_t Spi_ChannelType;
typedef uint8_t Spi_JobType;
typedef uint8_t Spi_SequenceType;

#endif /* SPI_CFG_H */
