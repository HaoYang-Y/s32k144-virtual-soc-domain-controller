/*
 * @brief 配置管理器头文件 (TODO 骨架)
 *        负责加载 YAML 配置文件，管理 CAN/信号/UDS 等配置
 *
 * @note 对应书籍：后续 SOME/IP 学习章节
 *       配置驱动开发学习模块
 *
 * TODO：看书后实现 ConfigManager 类
 *       1. 学习 yaml-cpp 库的基本用法
 *       2. 实现 YAML 配置文件的加载和解析
 *       3. 实现 CAN 信号定义表的解析
 *       4. 实现 UDS DID 定义表的解析
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace domain_controller {

/** @brief CAN 信号描述结构体 */
struct CanSignalDef {
    std::string name;            /**< 信号名称，如 "VehicleSpeed" */
    uint32_t    can_id;          /**< CAN 报文 ID (标准帧 11bit) */
    uint8_t     start_bit;       /**< 起始位 (0~63) */
    uint8_t     length;          /**< 位长度 (1~32) */
    bool        is_big_endian;   /**< true=BigEndian, false=LittleEndian */
    double      scale;           /**< 缩放因子 */
    double      offset;          /**< 偏移量 */
    std::string unit;            /**< 单位，如 "km/h" */
};

/** @brief UDS DID 定义结构体 */
struct DidDef {
    uint16_t    did;         /**< 数据标识符 (0x0000~0xFFFF) */
    std::string name;        /**< DID 名称 */
    uint8_t     length;      /**< 数据字节数 */
    std::string unit;        /**< 单位 */
};

/**
 * @brief 配置管理器类
 *
 * TODO：学习 yaml-cpp 后实现以下函数
 *       关键思路：
 *         1. 构造函数接收配置文件路径，加载 YAML
 *         2. can_signals() 解析 signals 下的每个条目
 *         3. uds_dids() 解析 uds/dids 下的每个条目
 *         4. validate() 检查必要字段是否存在
 */
class ConfigManager {
public:
    explicit ConfigManager(const std::string& config_path);
    ~ConfigManager() = default;

    /* -- CAN 配置 -- */
    std::string               can_interface()       const;
    uint32_t                  can_bitrate()         const;
    std::vector<CanSignalDef> can_signals()         const;

    /* -- UDS 配置 -- */
    std::vector<DidDef>       uds_dids()            const;

    /* -- 服务器配置 -- */
    uint16_t                  http_port()           const;
    std::string               log_level()           const;

private:
    /* TODO：定义成员变量 */
    // void* root_;  /* YAML::Node 需要包含 yaml-cpp 头文件 */

    /**
     * @brief 校验配置完整性
     * @return true=有效, false=缺少必要字段
     */
    bool validate(void) const;
};

} /* namespace domain_controller */
