/*
 * @brief CAN 总线管理器实现 (TODO 骨架)
 *        封装 Linux SocketCAN API，提供 CAN 帧收发
 *
 * @note 对应书籍：后续 CAN 应用层协议学习章节
 *       底层使用 AF_CAN / SOCK_RAW
 *
 * TODO：看书后实现以下函数
 *       1. open_socket() — 打开并绑定 SocketCAN 套接字
 *       2. send()        — 发送 CAN 帧
 *       3. recv_loop()   — 接收线程主循环
 *       4. register_rx_callback() — 注册接收回调
 */
#include "can_manager.h"

#include <cstring>
#include <cerrno>
#include <cstdio>

namespace domain_controller {

/* ===================================================================
 * CanFrame — CAN 数据帧结构封装
 * =================================================================== */

/*
 * TODO：实现 CanFrame 构造函数和 to_string()
 * 提示：
 *   CanFrame::CanFrame(uint32_t id, const std::vector<uint8_t>& d, bool ext)
 *       : can_id(id), dlc(static_cast<uint8_t>(d.size())), is_extended(ext) {
 *       // 复制数据到 data 数组
 *   }
 *
 *   std::string CanFrame::to_string() const {
 *       // 格式化输出 "0x123 [8] 01 23 45 67 89 AB CD EF"
 *   }
 */

/* ===================================================================
 * CanBus — SocketCAN 封装
 * =================================================================== */

/*
 * TODO：实现 CanBus 构造函数和析构函数
 */
CanBus::CanBus(const std::string& interface_name, uint32_t bitrate)
    : ifname_(interface_name), sock_fd_(-1) {
    (void)bitrate;
}

CanBus::~CanBus() {
    stop();
}

/*
 * TODO：实现 CanBus 打开 Socket 连接
 * 步骤：
 *   1. socket(PF_CAN, SOCK_RAW, CAN_RAW)
 *   2. ioctl() 获取接口索引
 *   3. bind() 绑定到 CAN 接口
 *   4. 返回 0 或 -1
 */
void CanBus::start() {
    /* TODO：open_socket() + 启动接收线程 */
}

/*
 * TODO：停止 CAN 总线
 */
void CanBus::stop() {
    /* TODO：停止接收线程 + 关闭 socket */
}

/*
 * TODO：发送 CAN 帧
 * 步骤：
 *   1. 构造 canfd_frame 结构体
 *   2. write() 发送
 *   3. 返回 0 或 -1
 */
int CanBus::send(const CanFrame& frame) {
    (void)frame;
    /* TODO：实现 CAN 帧发送 */
    return 0;
}

/*
 * TODO：注册接收回调
 */
void CanBus::register_rx_callback(uint32_t can_id, uint32_t mask,
                                  CanBus::RxCallback callback) {
    (void)can_id;
    (void)mask;
    (void)callback;
    /* TODO：保存回调到列表 */
}

const std::string& CanBus::interface_name() const {
    return ifname_;
}

/* ===================================================================
 * 工厂函数
 * =================================================================== */

/*
 * TODO：实现工厂函数
 */
std::unique_ptr<CanBus> create_can_bus(const std::string& interface_name,
                                       uint32_t bitrate) {
    /* TODO：返回 new CanBus(...) */
    (void)interface_name;
    (void)bitrate;
    return nullptr;
}

} /* namespace domain_controller */
