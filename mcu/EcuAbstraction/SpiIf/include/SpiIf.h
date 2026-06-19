/**
 * @file    SpiIf.h
 * @brief   [SKELETON] SPI Interface — AUTOSAR CP ECU Abstraction 层
 *
 * @note    封装 MCAL Spi 驱动 (LPSPI0)，为上层提供 SPI 通信总线抽象。
 *          当前用于 MCU ↔ SOC (FT2232H) 的 SPI 数据交换。
 */

#ifndef SPIIF_H
#define SPIIF_H

#include "Std_Types.h"

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief SPI 帧固定长度 (与 SOC 约定 32 字节) */
#define SPIIF_FRAME_LENGTH          32U

/** @brief SPI 帧头部命令字 */
#define SPIIF_CMD_DATA              0xAAU
#define SPIIF_CMD_ACK               0x55U

/** @brief SPI 帧结构 */
typedef struct {
    uint8_t  cmd;           /* 命令字节 */
    uint8_t  payload[30];   /* 负载数据 */
    uint8_t  crc;           /* CRC8 校验 */
} SpiIf_FrameType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

void    SpiIf_Init(void);
void    SpiIf_Transmit(const SpiIf_FrameType *Frame);
void    SpiIf_Receive(SpiIf_FrameType *Frame);
void    SpiIf_Exchange(const SpiIf_FrameType *TxFrame, SpiIf_FrameType *RxFrame);

#endif /* SPIIF_H */
