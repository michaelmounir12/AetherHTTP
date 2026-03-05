#pragma once

#include <mutex>
#include <string_view>

class Logger {
public:
  enum class Level { Debug, Info, Warn, Error };

  static Logger& instance();

  void set_level(Level level);
  void log(Level level, std::string_view message);

  static const char* level_to_string(Level level);

private:
  Logger() = default;

  std::mutex mutex_;
  Level min_level_ = Level::Info;
};

