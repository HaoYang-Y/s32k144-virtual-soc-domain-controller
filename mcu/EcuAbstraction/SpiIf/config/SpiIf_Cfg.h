/**
 * @file    SpiIf_Cfg.h
 * @brief   SPI Interface 配置 — SPI 通道与时序
 */

#ifndef SPIIF_CFG_H
#define SPIIF_CFG_H

#include "Std_Types.h"

/** @brief SPI 通信参数 (与 SOC FT2232H 匹配) */
#define SPIIF_FRAME_SIZE_BYTES      32U
#define SPIIF_DEFAULT_FREQ_HZ       5000000UL   /* 5 MHz */
#define SPIIF_CRC8_POLY             0x07

/** @brief SPI 帧命令字 */
#define SPIIF_CMD_DATA              0xAAU
#define SPIIF_CMD_ACK               0x55U
#define SPIIF_CMD_CONFIG            0xCCU

#endif /* SPIIF_CFG_H */