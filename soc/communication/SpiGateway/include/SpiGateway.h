/**
 * @file    SpiGateway.h
 * @brief   [SKELETON] SPI 网关 — SOC 侧 SPI ↔ 内部消息桥接
 *
 * @note    接收来自 MCU 的 SPI 帧，解析后转发给上层服务
 *          (SignalFusion → ara::com)
 *          上层也可以通过 SpiGateway 下发命令给 MCU
 */

#ifndef SOC_SPIGATEWAY_H
#define SOC_SPIGATEWAY_H

#include <cstdint>
#include <functional>
#include <array>

namespace domain_controller {

/** @brief SPI 帧结构 (与 MCU SpiIf_FrameType 协议一致) */
struct SpiFrame {
    uint8_t  cmd;
    uint8_t  payload[30];
    uint8_t  crc;
};

/** @brief 帧命令定义 */
enum SpiCmd : uint8_t {
    kSpiCmdData   = 0xAA,   /* CAN 数据帧 (MCU→SOC) */
    kSpiCmdAck    = 0x55,   /* 确认帧 (SOC→MCU) */
    kSpiCmdConfig = 0xCC,   /* 配置下发 (SOC→MCU) */
};

/** @brief SPI 帧接收回调 */
using SpiFrameCallback = std::function<void(const SpiFrame&)>;

/**
 * @brief SPI 网关类
 */
class SpiGateway {
public:
    SpiGateway() = default;

    /** @brief 初始化 SPI 网关 */
    bool Init(const std::string& spi_device, int freq_hz);

    /** @brief 启动轮询线程 */
    bool Start();

    /** @brief 停止 */
    void Stop();

    /** @brief 发送帧给 MCU */
    bool SendFrame(const SpiFrame& frame);

    /** @brief 注册接收回调 */
    void RegisterRxCallback(SpiFrameCallback cb);

private:
    /* TODO: 轮询线程: 定时 SpiDriver::Exchange */
    SpiFrameCallback rx_callback_;
    bool running_ = false;
};

} /* namespace domain_controller */

#endif /* SOC_SPIGATEWAY_H */
