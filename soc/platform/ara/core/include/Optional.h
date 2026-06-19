/**
 * @file    Optional.h
 * @brief   [SKELETON] ara::core::Optional — AUTOSAR AP 可选值类型
 *
 * @note    对应 AUTOSAR AP ara::core::Optional<T>
 *          类似 std::optional 的简单封装
 */

#ifndef ARA_CORE_OPTIONAL_H
#define ARA_CORE_OPTIONAL_H

#include <type_traits>

namespace ara {
namespace core {

template <typename T>
class Optional {
public:
    Optional() : has_value_(false) {}
    Optional(const T& value) : value_(value), has_value_(true) {}

    bool HasValue() const { return has_value_; }
    const T& Value() const { return value_; }
    T       ValueOr(const T& default_value) const {
        return has_value_ ? value_ : default_value;
    }

private:
    T    value_;
    bool has_value_;
};

} /* namespace core */
} /* namespace ara */

#endif /* ARA_CORE_OPTIONAL_H */
