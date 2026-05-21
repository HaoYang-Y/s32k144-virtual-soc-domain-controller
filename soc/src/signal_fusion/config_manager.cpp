/*
 * @brief 配置管理器实现 (TODO 骨架)
 *        解析 YAML 配置文件，加载 CAN/UDS/服务器配置
 *
 * @note 对应书籍：后续 SOME/IP 学习章节
 *       使用 yaml-cpp 库解析 YAML
 *
 * TODO：看书后实现 ConfigManager 类
 *       1. 构造函数加载 YAML 文件
 *       2. can_interface() / can_bitrate() / can_signals()
 *       3. uds_dids() / http_port() / log_level()
 *       4. validate() 检查必要字段
 */
#include "config_manager.h"

namespace domain_controller {

/*
 * TODO：实现构造函数 — 加载 YAML 配置文件
 * 提示：使用 YAML::LoadFile()
 */
ConfigManager::ConfigManager(const std::string& config_path) {
    (void)config_path;
    /* TODO：加载 YAML 并校验 */
}

/*
 * TODO：返回 CAN 接口名称，如 "can0"
 */
std::string ConfigManager::can_interface() const {
    /* TODO：从配置中读取 */
    return "can0";
}

/*
 * TODO：返回 CAN 波特率，如 500000
 */
uint32_t ConfigManager::can_bitrate() const {
    /* TODO：从配置中读取 */
    return 500000;
}

/*
 * TODO：返回 CAN 信号定义表
 * 解析 signals 列表中的每个条目:
 *   - name / can_id / start_bit / length
 *   - endian (big/little) / scale / offset / unit
 */
std::vector<CanSignalDef> ConfigManager::can_signals() const {
    std::vector<CanSignalDef> signals;
    /* TODO：遍历 signals 列表并填充 */
    return signals;
}

/*
 * TODO：返回 UDS DID 定义表
 */
std::vector<DidDef> ConfigManager::uds_dids() const {
    std::vector<DidDef> dids;
    /* TODO：遍历 dids 列表并填充 */
    return dids;
}

/*
 * TODO：返回 HTTP 服务端口
 */
uint16_t ConfigManager::http_port() const {
    /* TODO：从配置中读取 */
    return 8080;
}

/*
 * TODO：返回日志级别
 */
std::string ConfigManager::log_level() const {
    /* TODO：从配置中读取 */
    return "info";
}

/*
 * TODO：校验配置完整性
 * 检查 can.interface、can.bitrate、server.http_port 等关键字段
 */
bool ConfigManager::validate(void) const {
    /* TODO：检查必要字段是否存在 */
    return true;
}

} /* namespace domain_controller */
