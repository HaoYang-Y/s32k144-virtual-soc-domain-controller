/**
 * @file    SomeIpBinding.h
 * @brief   [SKELETON] ara::com — SOME/IP 协议绑定
 *
 * @note    对应 AUTOSAR AP ara::com SOME/IP Binding
 *          处理 SOME/IP 消息头封装/解封 (Message ID, Length, Request ID...)
 */

#ifndef ARA_COM_SOMEIPBINDING_H
#define ARA_COM_SOMEIPBINDING_H

#include <cstdint>
#include <vector>
#include <functional>

namespace ara {
namespace com {

/** @brief SOME/IP 头部 */
struct SomeIpHeader {
    uint16_t service_id;
    uint16_t method_id;
    uint32_t length;
    uint16_t client_id;
    uint16_t session_id;
    uint8_t  protocol_version   = 1;
    uint8_t  interface_version  = 1;
    uint8_t  message_type       = 0x00; /* 0x00=REQUEST, 0x02=RESPONSE */
    uint8_t  return_code        = 0x00;
};

/** @brief SOME/IP 消息 */
struct SomeIpMessage {
    SomeIpHeader       header;
    std::vector<uint8_t> payload;
};

/**
 * @brief SOME/IP 编解码器
 */
class SomeIpBinding {
public:
    SomeIpBinding() = default;

    /** @brief 编码 SOME/IP 消息为字节流 */
    std::vector<uint8_t> Encode(const SomeIpMessage& msg);

    /** @brief 从字节流解码 SOME/IP 消息 */
    SomeIpMessage Decode(const std::vector<uint8_t>& raw);

private:
    uint16_t next_session_id_ = 0;
};

} /* namespace com */
} /* namespace ara */

#endif /* ARA_COM_SOMEIPBINDING_H */
