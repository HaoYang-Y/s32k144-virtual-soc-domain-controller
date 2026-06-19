/**
 * @file    SpiDriver.cpp
 * @brief   [SKELETON] SPI 驱动实现
 */

#include "SpiDriver.h"
#include <cstring>
#include <cerrno>
#include <cstdio>

namespace domain_controller {

SpiDriver::~SpiDriver() { Deinit(); }

bool SpiDriver::Init(const std::string& device, int frequency_hz) {
    (void)device;
    (void)frequency_hz;
    /* TODO: libmpsse 初始化 */
    /*   handle_ = MPSSE(SPI0, frequency_hz, MSB); */
    return false;
}

bool SpiDriver::Exchange(const uint8_t* tx_buf, uint8_t* rx_buf, size_t length) {
    (void)tx_buf;
    (void)rx_buf;
    (void)length;
    /* TODO: 全双工 SPI 传输 */
    return false;
}

void SpiDriver::Deinit() {
    /* TODO: 关闭 SPI 设备 */
    handle_ = nullptr;
}

} /* namespace domain_controller */
