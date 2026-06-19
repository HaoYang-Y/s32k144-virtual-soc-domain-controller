/**
 * @file    Logger.cpp
 * @brief   [SKELETON] ara::log::Logger 实现
 */

#include "Logger.h"
#include <iostream>
#include <chrono>
#include <ctime>

namespace ara {
namespace log {

static const char* LevelToString(LogLevel lv) {
    switch (lv) {
        case LogLevel::kFatal:   return "FATAL";
        case LogLevel::kError:   return "ERROR";
        case LogLevel::kWarn:    return "WARN";
        case LogLevel::kInfo:    return "INFO";
        case LogLevel::kVerbose: return "VERB";
        case LogLevel::kDebug:   return "DEBUG";
        default:                 return "??";
    }
}

void Logger::Log(LogLevel level, const std::string& msg) {
    if (level > level_) return;
    auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::cerr << "[" << LevelToString(level) << "][" << context_
              << "] " << msg << std::endl;
}

void Logger::Info(const std::string& msg)  { Log(LogLevel::kInfo, msg); }
void Logger::Warn(const std::string& msg)  { Log(LogLevel::kWarn, msg); }
void Logger::Error(const std::string& msg) { Log(LogLevel::kError, msg); }
void Logger::Debug(const std::string& msg){ Log(LogLevel::kDebug, msg); }

std::ostringstream& Logger::LogStream(LogLevel level) {
    stream_.str("");
    stream_.clear();
    /* TODO: 实际实现需处理流结束时的自动输出 */
    return stream_;
}

} /* namespace log */
} /* namespace ara */
