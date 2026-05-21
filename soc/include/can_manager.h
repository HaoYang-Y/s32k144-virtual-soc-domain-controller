/*
 * @brief CAN 总线管理器头文件 (TODO 骨架)
 *        封装 Linux SocketCAN 收发操作
 *
 * @note 对应书籍：后续 CAN 应用层协议学习章节
 *       基于 Linux SocketCAN (AF_CAN / SOCK_RAW)
 *
 * TODO：看书后实现 CanBus 类
 *       1. 学习 SocketCAN 编程 (socket, bind, send, recv)
 *       2. 实现 CAN 帧的发送和接收
 *       3. 实现接收线程和回调机制
 *       4. 理解 CAN 过滤规则设置
 */
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <array>

namespace domain_controller {

/** @brief CAN 标准帧数据最大长度 */
static constexpr uint8_t CAN_DATA_LEN = 8;

/** @brief CAN FD 帧数据最大长度 */
static constexpr uint8_t CANFD_DATA_LEN = 64;

/**
 * @brief CAN 数据帧封装
 *
 * TODO：实现 CanFrame 结构体
 *       关键字段：
 *         - can_id:    CAN 消息 ID (11bit 标准帧 / 29bit 扩展帧)
 *         - dlc:       数据长度代码 (0~8)
 *         - data:      帧数据
 *         - is_extended: 扩展帧标志
 */
struct CanFrame {
    uint32_t              can_id;       /**< CAN 消息 ID */
    uint8_t               dlc;          /**< 数据长度 (0~8) */
    std::array<uint8_t, CANFD_DATA_LEN> data; /**< 帧数据 */
    bool                  is_extended;  /**< true=扩展帧(29bit) */

    CanFrame();
    CanFrame(uint32_t id, const std::vector<uint8_t>& d, bool ext = false);

    /** @brief 转换为可读字符串 */
    std::string to_string() const;
};

/**
 * @brief CAN 总线管理器
 *
 * TODO：看书后实现以下函数
 *       1. start()   — 打开 socket + bind + 启动接收线程
 *       2. stop()    — 停止接收线程 + 关闭 socket
 *       3. send()    — 发送 CAN 帧
 *       4. register_rx_callback() — 注册接收回调
 */
class CanBus {
public:
    /** @brief CAN 帧接收回调类型 */
    using RxCallback = std::function<void(const CanFrame&)>;

    explicit CanBus(const std::string& interface_name,
                    uint32_t bitrate = 500000);
    ~CanBus();

    CanBus(const CanBus&) = delete;
    CanBus& operator=(const CanBus&) = delete;

    void start();
    void stop();
    int  send(const CanFrame& frame);
    void register_rx_callback(uint32_t can_id, uint32_t mask,
                              RxCallback callback);

    const std::string& interface_name() const;

private:
    std::string ifname_;  /**< CAN 接口名称 */
    int         sock_fd_; /**< SocketCAN 文件描述符 */

    /* TODO：添加需要的成员变量 */
    // std::thread  recv_thread_;
    // std::atomic<bool> running_;
};

/**
 * @brief 创建 CanBus 实例的工厂函数
 */
std::unique_ptr<CanBus> create_can_bus(const std::string& interface_name,
                                       uint32_t bitrate = 500000);

} /* namespace domain_controller */
