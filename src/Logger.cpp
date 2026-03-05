#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger& Logger::instance() {
  static Logger inst;
  return inst;
}

void Logger::set_level(Level level) {
  std::lock_guard<std::mutex> lock(mutex_);
  min_level_ = level;
}

const char* Logger::level_to_string(Level level) {
  switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO";
    case Level::Warn: return "WARN";
    case Level::Error: return "ERROR";
  }
  return "UNKNOWN";
}

void Logger::log(Level level, std::string_view message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (static_cast<int>(level) < static_cast<int>(min_level_)) return;

  const auto now = std::chrono::system_clock::now();
  const std::time_t tt = std::chrono::system_clock::to_time_t(now);

  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif

  std::ostringstream ts;
  ts << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

  std::cerr << "[" << ts.str() << "] [" << level_to_string(level) << "] " << message << "\n";
}

