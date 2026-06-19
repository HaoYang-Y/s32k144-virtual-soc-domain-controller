/**
 * @file    SpiGateway.cpp
 * @brief   [SKELETON] SPI 网关实现
 */

#include "SpiGateway.h"

namespace domain_controller {

bool SpiGateway::Init(const std::string& spi_device, int freq_hz) {
    (void)spi_device;
    (void)freq_hz;
    /* TODO: 初始化 SpiDriver 实例 */
    return false;
}

bool SpiGateway::Start() {
    /* TODO: 启动接收线程 */
    running_ = true;
    return true;
}

void SpiGateway::Stop() {
    running_ = false;
}

bool SpiGateway::SendFrame(const SpiFrame& frame) {
    (void)frame;
    /* TODO: 通过 SpiDriver 发送帧 */
    return false;
}

void SpiGateway::RegisterRxCallback(SpiFrameCallback cb) {
    rx_callback_ = cb;
}

} /* namespace domain_controller */
