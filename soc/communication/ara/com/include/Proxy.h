/**
 * @file    Proxy.h
 * @brief   [SKELETON] ara::com::Proxy — AUTOSAR AP 服务代理基类
 *
 * @note    对应 AUTOSAR AP ara::com::Proxy
 *          客户端通过 Proxy 与服务端通信（SOME/IP 或 IPC）
 */

#ifndef ARA_COM_PROXY_H
#define ARA_COM_PROXY_H

#include <string>
#include <memory>
#include "InstanceIdentifier.h"

namespace ara {
namespace com {

/**
 * @brief 服务代理基类
 * @tparam T 具体服务接口类型
 */
template <typename T>
class Proxy {
public:
    virtual ~Proxy() = default;

    /** @brief 获取服务实例 ID */
    virtual InstanceIdentifier GetInstanceId() const = 0;

    /** @brief 处理请求 */
    virtual void HandleRequest(const T& request) = 0;

protected:
    Proxy() = default;
};

} /* namespace com */
} /* namespace ara */

#endif /* ARA_COM_PROXY_H */
