/**
 * @file    Result.h
 * @brief   [SKELETON] ara::core::Result — AUTOSAR AP 核心类型
 *
 * @note    对应 AUTOSAR AP ara::core::Result<T, E>
 *          类似 C++23 std::expected 的 ErrorOr 模式
 *          当前为简化骨架，仅提供基本功能
 */

#ifndef ARA_CORE_RESULT_H
#define ARA_CORE_RESULT_H

#include <type_traits>
#include <utility>

namespace ara {
namespace core {

/**
 * @brief 简易 Result 类型
 * @tparam T  值类型
 * @tparam E  错误类型（默认 int）
 */
template <typename T, typename E = int>
class Result {
public:
    /* 构造成功值 */
    Result(const T& value) : value_(value), has_value_(true) {}
    Result(T&& value) : value_(std::move(value)), has_value_(true) {}

    /* 构造错误 */
    Result(const E& error) : error_(error), has_value_(false) {}

    bool HasValue() const { return has_value_; }
    bool HasError() const { return !has_value_; }

    T&    Value()       { return value_; }
    const T& Value() const { return value_; }
    E     Error() const { return error_; }

    /* value_or 风格 */
    T ValueOr(const T& default_value) const {
        return has_value_ ? value_ : default_value;
    }

private:
    T   value_;
    E   error_;
    bool has_value_;
};

} /* namespace core */
} /* namespace ara */

#endif /* ARA_CORE_RESULT_H */
