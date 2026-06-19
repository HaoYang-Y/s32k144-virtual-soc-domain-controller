/**
 * @file    SpiDriver.h
 * @brief   [SKELETON] SPI 驱动封装 — SOC 侧 (FT2232H + libmpsse)
 *
 * @note    非 AUTOSAR AP 标准模块，但在实际工程中 SOC 通过 SPI 与 MCU 通信
 *          是 SOC 侧 "Platform" 层的重要组成部分
 *          底层依赖 libmpsse (FTDI MPSSE) 库
 */

#ifndef SOC_SPI_DRIVER_H
#define SOC_SPI_DRIVER_H

#include <cstdint>
#include <vector>
#include <string>

namespace domain_controller {

/** @brief SPI 帧参数 (与 MCU 侧 SpiIf_FrameType 匹配) */
static constexpr size_t kSpiFrameLength = 32U;

/**
 * @brief SPI 驱动类
 *
 * TODO: 学习 libmpsse 后实现
 *       1. mpsse_open() / mpsse_close()
 *       2. mpsse_transfer() 全双工收发
 *       3. 帧同步与重试机制
 */
class SpiDriver {
public:
    SpiDriver() = default;
    ~SpiDriver();

    /** @brief 初始化 SPI 设备 */
    bool Init(const std::string& device, int frequency_hz);

    /** @brief 收发 32 字节 SPI 帧 */
    bool Exchange(const uint8_t* tx_buf, uint8_t* rx_buf, size_t length = kSpiFrameLength);

    /** @brief 关闭 SPI 设备 */
    void Deinit();

private:
    void* handle_ = nullptr;  /* FTDI device handle (mpsse context) */
};

} /* namespace domain_controller */

#endif /* SOC_SPI_DRIVER_H */
