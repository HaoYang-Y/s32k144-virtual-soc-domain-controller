/**
 * @file    UdsServer.h
 * @brief   [SKELETON] ara::diag::UdsServer — AUTOSAR AP UDS 诊断服务
 *
 * @note    对应 AUTOSAR AP ara::diag 规范
 *          实现 UDS (ISO 14229) 诊断协议处理器
 *          包括: 会话控制、DID 读写、例程控制、DTC 操作
 */

#ifndef ARA_DIAG_UDSSERVER_H
#define ARA_DIAG_UDSSERVER_H

#include <cstdint>
#include <vector>
#include <functional>
#include <map>

namespace ara {
namespace diag {

/** @brief UDS 诊断会话 */
enum class DiagSessionType : uint8_t {
    kDefault      = 0x01,
    kProgramming  = 0x02,
    kExtended     = 0x03,
    kSafety       = 0x04,
};

/** @brief UDS 服务 ID */
enum UdsServiceId : uint8_t {
    kSidDiagSessionCtrl  = 0x10,
    kSidEcuReset         = 0x11,
    kSidReadDataById     = 0x22,
    kSidWriteDataById    = 0x2E,
    kSidRoutineControl   = 0x31,
    kSidTesterPresent    = 0x3E,
};

/** @brief 诊断响应 */
struct UdsResponse {
    uint8_t              sid;
    uint8_t              response_code;
    std::vector<uint8_t> data;
};

/**
 * @brief UDS 服务器
 */
class UdsServer {
public:
    UdsServer() = default;

    /** @brief 处理诊断请求 */
    UdsResponse HandleRequest(const std::vector<uint8_t>& request);

    /** @brief 注册 DID 读取回调 */
    void RegisterDidReader(uint16_t did,
        std::function<std::vector<uint8_t>()> reader);

    /** @brief 注册 DID 写入回调 */
    void RegisterDidWriter(uint16_t did,
        std::function<bool(const std::vector<uint8_t>&)> writer);

private:
    DiagSessionType current_session_ = DiagSessionType::kDefault;
    std::map<uint16_t, std::function<std::vector<uint8_t>()>>       did_readers_;
    std::map<uint16_t, std::function<bool(const std::vector<uint8_t>&)>> did_writers_;
};

} /* namespace diag */
} /* namespace ara */

#endif /* ARA_DIAG_UDSSERVER_H */
