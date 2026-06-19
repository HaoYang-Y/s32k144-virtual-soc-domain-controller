/**
 * @file    UdsServer.cpp
 * @brief   [SKELETON] UDS 诊断服务实现
 */

#include "UdsServer.h"

namespace ara {
namespace diag {

UdsResponse UdsServer::HandleRequest(const std::vector<uint8_t>& request) {
    (void)request;
    /* TODO: 解析请求 SID，分发到对应处理器 */
    return {};
}

void UdsServer::RegisterDidReader(uint16_t did,
    std::function<std::vector<uint8_t>()> reader) {
    did_readers_[did] = reader;
}

void UdsServer::RegisterDidWriter(uint16_t did,
    std::function<bool(const std::vector<uint8_t>&)> writer) {
    did_writers_[did] = writer;
}

} /* namespace diag */
} /* namespace ara */
