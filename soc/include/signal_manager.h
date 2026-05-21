/*
 * @brief 信号管理器头文件 (TODO 骨架)
 *        CAN 信号解析与融合 — 从原始 CAN 帧数据中提取物理值
 *
 * @note 对应书籍：后续信号处理学习章节
 *       支持 Motorola(BigEndian) 和 Intel(LittleEndian) 字节序
 *
 * TODO：看书后实现 SignalParser 和 SignalFusion 类
 *       1. 理解信号位域提取算法
 *       2. 实现跨字节位域解析
 *       3. 实现信号值变化通知机制
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include "config_manager.h"

namespace domain_controller {

/** @brief 信号值变化事件回调类型 */
using SignalChangeCallback = std::function<void(
    const std::string& signal_name,
    double old_value,
    double new_value)>;

/**
 * @brief 车辆信号状态 — 最新物理值和时间戳
 */
struct SignalValue {
    double      value;          /**< 物理值 (已乘 scale + offset) */
    uint64_t    timestamp_ms;   /**< 接收时间戳 (毫秒) */
    bool        valid;          /**< 数据有效标志 */
};

/**
 * @brief CAN 信号解析器
 *
 * TODO：看书后实现信号提取
 *       核心算法：从字节数组中提取指定位域
 *       - Intel 格式 (LittleEndian):
 *         起始位 = 字节号 * 8 + 位号，LSB 对齐
 *       - Motorola 格式 (BigEndian):
 *         起始位 = 字节号 * 8 + (7 - 位号)，MSB 对齐
 */
class SignalParser {
public:
    SignalParser() = default;

    /**
     * @brief 从 CAN 帧数据中提取单个信号
     *
     * TODO：实现信号提取算法
     * @param[in]  data  CAN 帧 8 字节原始数据
     * @param[in]  def   信号定义
     * @return     解析出的物理值 (raw * scale + offset)
     */
    double extract_signal(const std::array<uint8_t, 8>& data,
                          const CanSignalDef& def) const;

    /**
     * @brief 将物理值编码为 CAN 帧数据
     *
     * TODO：实现信号编码算法 (extract_signal 的逆过程)
     * @param[out] data      目标 CAN 帧数据缓冲区
     * @param[in]  def       信号定义
     * @param[in]  phys_value 物理值
     * @return     true=编码成功
     */
    bool encode_signal(std::array<uint8_t, 8>& data,
                       const CanSignalDef& def,
                       double phys_value) const;

private:
    /**
     * @brief 从字节数组中提取位域 (低位对齐)
     *
     * TODO：实现位域提取核心算法
     */
    uint64_t extract_raw(const uint8_t* data, uint8_t start_bit,
                         uint8_t length, bool big_endian) const;

    /**
     * @brief 将原始值写入字节数组的指定位域
     *
     * TODO：实现位域编码核心算法
     */
    void encode_raw(uint8_t* data, uint8_t start_bit,
                    uint8_t length, bool big_endian,
                    uint64_t raw) const;
};

/**
 * @brief 信号融合中心
 *
 * TODO：看书后实现信号管理
 *       1. 根据 CAN ID 分发帧到对应信号
 *       2. 维护信号最新值缓存
 *       3. 支持信号变化通知回调
 */
class SignalFusion {
public:
    explicit SignalFusion(const std::vector<CanSignalDef>& signal_defs);

    /**
     * @brief 处理接收到的 CAN 帧，解析其中的所有信号
     */
    void process_can_frame(uint32_t can_id,
                           const std::array<uint8_t, 8>& frame_data);

    /**
     * @brief 获取指定信号的最新值
     */
    SignalValue get_signal(const std::string& signal_name) const;

    /**
     * @brief 注册信号值变化回调
     */
    void register_change_callback(SignalChangeCallback callback);

    /**
     * @brief 打印当前所有信号状态
     */
    void dump_signals() const;

    /**
     * @brief 获取所有信号名称
     */
    std::vector<std::string> signal_names() const;

private:
    /* TODO：定义成员变量 */
    SignalParser parser_;
};

} /* namespace domain_controller */
