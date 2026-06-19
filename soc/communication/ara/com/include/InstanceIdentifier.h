/**
 * @file    InstanceIdentifier.h
 * @brief   [SKELETON] ara::com 服务实例标识
 *
 * @note    对应 AUTOSAR AP ara::com::InstanceIdentifier
 *          标识服务实例的唯一句柄
 */

#ifndef ARA_COM_INSTANCEIDENTIFIER_H
#define ARA_COM_INSTANCEIDENTIFIER_H

#include <string>

namespace ara {
namespace com {

class InstanceIdentifier {
public:
    explicit InstanceIdentifier(const std::string& id) : id_(id) {}
    const std::string& ToString() const { return id_; }

private:
    std::string id_;
};

} /* namespace com */
} /* namespace ara */

#endif /* ARA_COM_INSTANCEIDENTIFIER_H */
