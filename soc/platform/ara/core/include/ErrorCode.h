/**
 * @file    ErrorCode.h
 * @brief   [SKELETON] ara::core::ErrorCode — AUTOSAR AP 错误码
 *
 * @note    对应 AUTOSAR AP ara::core::ErrorCode
 *          包含错误域、错误值和辅助信息
 */

#ifndef ARA_CORE_ERRORCODE_H
#define ARA_CORE_ERRORCODE_H

#include <string>

namespace ara {
namespace core {

class ErrorCode {
public:
    ErrorCode(int value = 0, const std::string& message = "")
        : value_(value), message_(message) {}

    int Value() const { return value_; }
    const std::string& Message() const { return message_; }

private:
    int         value_;
    std::string message_;
};

} /* namespace core */
} /* namespace ara */

#endif /* ARA_CORE_ERRORCODE_H */
