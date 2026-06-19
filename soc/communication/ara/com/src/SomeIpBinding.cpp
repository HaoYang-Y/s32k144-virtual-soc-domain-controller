/**
 * @file    SomeIpBinding.cpp
 * @brief   [SKELETON] SOME/IP 协议绑定实现
 */

#include "SomeIpBinding.h"

namespace ara {
namespace com {

std::vector<uint8_t> SomeIpBinding::Encode(const SomeIpMessage& msg) {
    (void)msg;
    /* TODO: SOME/IP 消息序列化 */
    return {};
}

SomeIpMessage SomeIpBinding::Decode(const std::vector<uint8_t>& raw) {
    (void)raw;
    /* TODO: SOME/IP 消息反序列化 */
    return {};
}

} /* namespace com */
} /* namespace ara */
