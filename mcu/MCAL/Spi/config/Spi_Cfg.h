/**
 * @file    Spi_Cfg.h
 * @brief   SPI 预编译配置 + 配置实例声明
 *
 * @note    AUTOSAR 生成代码惯例: 配置头引用模块头 (Spi.h),
 *          以声明 extern 配置实例 Spi_Config (定义于 Spi_Cfg.c)
 */

#ifndef SPI_CFG_H
#define SPI_CFG_H

#include "Spi.h"

/* ===================================================================
 *  预编译配置宏
 * =================================================================== */

/** @brief 硬件 SPI 外设实例号 (SDK lpspi 驱动使用的 LPSPI1) */
#define SPI_INSTANCE_ID             1U

/** @brief 通道/作业/序列/硬件单元数量 (单从机学习场景) */
#define SPI_CHANNEL_COUNT           1U
#define SPI_JOB_COUNT               1U
#define SPI_SEQUENCE_COUNT          1U
#define SPI_HW_UNIT_COUNT           1U

/** @brief 单次传输最大数据长度 (字节, 与 SOC 侧 spidev 帧长一致) */
#define SPI_MAX_DATA_LEN            64U

/** @brief 开发错误检测开关 (DET: 入参检查门控) */
#define SPI_DEV_ERROR_DETECT        STD_ON

/** @brief 版本信息 API 开关 */
#define SPI_VERSION_INFO_API        STD_ON

/** @brief 软件版本号 (Spi_GetVersionInfo) */
#define SPI_SW_MAJOR_VERSION        1U
#define SPI_SW_MINOR_VERSION        0U
#define SPI_SW_PATCH_VERSION        0U

/** @brief 供应商 ID (工程占位值, 非 AUTOSAR 注册 vendorID) */
#define SPI_VENDOR_ID               99U

/** @brief AUTOSAR 标准模块 ID 表中 Spi = 84 */
#define SPI_MODULE_ID               84U

/* ===================================================================
 *  配置实例声明 (定义于 Spi_Cfg.c)
 * =================================================================== */

/** @brief SPI 默认配置实例 (预编译配置) */
extern const Spi_ConfigType Spi_Config;

#endif /* SPI_CFG_H */
