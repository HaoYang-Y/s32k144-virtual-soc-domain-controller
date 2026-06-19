/**
 * @file    ServiceDiscovery.h
 * @brief   [SKELETON] ara::com — SOME/IP 服务发现
 *
 * @note    对应 AUTOSAR AP ara::com + SOME/IP Service Discovery (AUTOSAR_SWS_SOMEIPSD)
 *          负责服务的 Offer/Find 流程管理
 */

#ifndef ARA_COM_SERVICEDISCOVERY_H
#define ARA_COM_SERVICEDISCOVERY_H

#include <cstdint>
#include <string>
#include <functional>

namespace ara {
namespace com {

/** @brief 服务发现事件回调 */
using SdEventHandler = std::function<void(uint16_t service_id,
                                           uint16_t instance_id,
                                           bool available)>;

class ServiceDiscovery {
public:
    ServiceDiscovery() = default;
    ~ServiceDiscovery() = default;

    /** @brief 初始化服务发现 (UDP 多播) */
    bool Init(const std::string& multicast_ip, uint16_t port);

    /** @brief 注册服务可用性回调 */
    void RegisterEventHandler(SdEventHandler handler);

    /** @brief 发送 OfferService 报文 */
    bool OfferService(uint16_t service_id, uint16_t instance_id);

    /** @brief 发送 FindService 报文 */
    bool FindService(uint16_t service_id, uint16_t instance_id);

private:
    /* TODO: SOME/IP-SD 报文编码/解码 */
    bool running_ = false;
    SdEventHandler event_handler_;
};

} /* namespace com */
} /* namespace ara */

#endif /* ARA_COM_SERVICEDISCOVERY_H */
