/**
 * @file    Skeleton.h
 * @brief   [SKELETON] ara::com::Skeleton — AUTOSAR AP 服务端骨架基类
 *
 * @note    对应 AUTOSAR AP ara::com::Skeleton
 *          服务端通过 Skeleton 提供服务实现（SOME/IP 或 IPC）
 */

#ifndef ARA_COM_SKELETON_H
#define ARA_COM_SKELETON_H

#include <string>
#include <memory>
#include "InstanceIdentifier.h"

namespace ara {
namespace com {

/**
 * @brief 服务端骨架基类
 * @tparam T 具体服务接口类型
 */
template <typename T>
class Skeleton {
public:
    virtual ~Skeleton() = default;

    /** @brief 启动服务 */
    virtual bool StartOfferService() = 0;

    /** @brief 停止服务 */
    virtual void StopOfferService() = 0;

    /** @brief 是否正在提供服务 */
    virtual bool IsServiceOffered() const = 0;

protected:
    Skeleton() = default;
};

} /* namespace com */
} /* namespace ara */

#endif /* ARA_COM_SKELETON_H */
