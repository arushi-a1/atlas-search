// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// Minimal thread-safe leveled logger. Not a replacement for a real
// logging library in production, but keeps the project dependency-free.

#ifndef DISTRIBUTED_SEARCH_COMMON_LOGGER_H_
#define DISTRIBUTED_SEARCH_COMMON_LOGGER_H_

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace dse {
namespace common {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

class Logger {
 public:
  static Logger& Instance() {
    static Logger instance;
    return instance;
  }

  void Log(LogLevel level, const std::string& component,
            const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::ostringstream out;
    out << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << " ["
        << LevelName(level) << "] (" << component << ") " << message;
    std::ostream& stream = (level == LogLevel::kError) ? std::cerr : std::cout;
    stream << out.str() << std::endl;
  }

 private:
  static const char* LevelName(LogLevel level) {
    switch (level) {
      case LogLevel::kDebug: return "DEBUG";
      case LogLevel::kInfo: return "INFO";
      case LogLevel::kWarn: return "WARN";
      case LogLevel::kError: return "ERROR";
    }
    return "?";
  }
  std::mutex mutex_;
};

#define DSE_LOG_INFO(component, msg) \
  dse::common::Logger::Instance().Log(dse::common::LogLevel::kInfo, component, msg)
#define DSE_LOG_WARN(component, msg) \
  dse::common::Logger::Instance().Log(dse::common::LogLevel::kWarn, component, msg)
#define DSE_LOG_ERROR(component, msg) \
  dse::common::Logger::Instance().Log(dse::common::LogLevel::kError, component, msg)
#define DSE_LOG_DEBUG(component, msg) \
  dse::common::Logger::Instance().Log(dse::common::LogLevel::kDebug, component, msg)

}  // namespace common
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_COMMON_LOGGER_H_
