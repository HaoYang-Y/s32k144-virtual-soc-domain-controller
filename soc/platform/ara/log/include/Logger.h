/**
 * @file    Logger.h
 * @brief   [SKELETON] ara::log::Logger — AUTOSAR AP 日志系统
 *
 * @note    对应 AUTOSAR AP ara::log 规范
 *          提供分级日志输出 (kWarn, kInfo, kVerbose, kError, kFatal)
 */

#ifndef ARA_LOG_LOGGER_H
#define ARA_LOG_LOGGER_H

#include <string>
#include <sstream>

namespace ara {
namespace log {

/** @brief 日志等级 */
enum class LogLevel {
    kOff,
    kFatal,
    kError,
    kWarn,
    kInfo,
    kVerbose,
    kDebug
};

/**
 * @brief 简易日志器
 */
class Logger {
public:
    Logger(const std::string& ctx, LogLevel level = LogLevel::kInfo)
        : context_(ctx), level_(level) {}

    void Log(LogLevel level, const std::string& msg);
    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);
    void Debug(const std::string& msg);

    void SetLevel(LogLevel level) { level_ = level; }

    /* 流式日志 */
    std::ostringstream& LogStream(LogLevel level);

private:
    std::string     context_;
    LogLevel        level_;
    std::ostringstream stream_;
};

} /* namespace log */
} /* namespace ara */

#endif /* ARA_LOG_LOGGER_H */
