/**
 * @file    SpiIf.c
 * @brief   [SKELETON] SPI Interface 实现
 *
 * @todo    通过 MCAL Spi 驱动完成 SPI 帧收发
 *          差分编码 + CRC8 校验 (参照 VehicleGateway_Design.md)
 */

#include "SpiIf.h"
/* TODO: #include "Spi.h" */

void SpiIf_Init(void)
{
    /* TODO: 初始化 MCAL Spi 驱动 */
}

void SpiIf_Transmit(const SpiIf_FrameType *Frame)
{
    (void)Frame;
    /* TODO: 调用 Spi_SyncTransmit 或 Spi_Exchange 发送帧 */
}

void SpiIf_Receive(SpiIf_FrameType *Frame)
{
    (void)Frame;
    /* TODO: 调用 Spi_ReadIB + Spi_Exchange 接收帧 */
}

void SpiIf_Exchange(const SpiIf_FrameType *TxFrame, SpiIf_FrameType *RxFrame)
{
    (void)TxFrame;
    (void)RxFrame;
    /* TODO: 全双工 SPI 传输 */
}
