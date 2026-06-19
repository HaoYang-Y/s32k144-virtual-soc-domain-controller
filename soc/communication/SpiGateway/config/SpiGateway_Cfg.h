/**
 * @file    SpiGateway_Cfg.h
 * @brief   SOC 侧 SPI 网关编译期配置
 *
 * @note    与 MCU 侧 SpiIf_Cfg.h 中的参数对应
 *          protobuf 消息定义见 proto/signal_gateway.proto
 */

#ifndef SOC_SPIGATEWAY_CFG_H
#define SOC_SPIGATEWAY_CFG_H

#include <cstdint>

/** @brief SPI 通信参数 */
static constexpr size_t kSpiFrameSize       = 32U;
static constexpr int    kSpiDefaultFreqHz   = 5000000;   /* 5 MHz */
static constexpr int    kSpiPollIntervalMs  = 10;         /* 轮询间隔 */

/** @brief FT2232H SPI 设备路径 */
static constexpr const char* kSpiDevicePath = "/dev/spidev0.0";

/** @brief protobuf 消息类型 ID (SPI 帧头) */
enum class SpiMsgType : uint8_t {
    kVehicleSignals = 0x01,
    kControlCommand = 0x02,
    kHeartbeat      = 0x03,
};

#endif /* SOC_SPIGATEWAY_CFG_H */