/**
 * @file    ServiceDiscovery.cpp
 * @brief   [SKELETON] SOME/IP 服务发现实现
 */

#include "ServiceDiscovery.h"

namespace ara {
namespace com {

bool ServiceDiscovery::Init(const std::string& multicast_ip, uint16_t port) {
    (void)multicast_ip;
    (void)port;
    /* TODO: 创建 UDP socket，加入多播组 */
    running_ = false;
    return false;
}

void ServiceDiscovery::RegisterEventHandler(SdEventHandler handler) {
    event_handler_ = handler;
}

bool ServiceDiscovery::OfferService(uint16_t service_id, uint16_t instance_id) {
    (void)service_id;
    (void)instance_id;
    /* TODO: 发送 OfferService 报文 (0xFF00 / entry_type=0x01) */
    return false;
}

bool ServiceDiscovery::FindService(uint16_t service_id, uint16_t instance_id) {
    (void)service_id;
    (void)instance_id;
    /* TODO: 发送 FindService 报文 */
    return false;
}

} /* namespace com */
} /* namespace ara */
