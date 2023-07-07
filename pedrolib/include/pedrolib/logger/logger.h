#ifndef PEDROLIB_LOGGER_LOGGER_H
#define PEDROLIB_LOGGER_LOGGER_H

#include "pedrolib/timestamp.h"
#include <fmt/color.h>
#include <mutex>
#include <string>

#define USE_STDLOG

#ifdef USE_SPDLOG
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#endif

namespace pedrolib {

class Logger {
public:
  enum class Level {
    kTrace,
    kInfo,
    kWarn,
    kError,
    kDisable,
  };

  static int Compare(Level x, Level y) noexcept {
    if (x == y) {
      return 0;
    }
    return static_cast<int>(x) < static_cast<int>(y) ? -1 : 1;
  }

#ifdef USE_SPDLOG
  std::shared_ptr<spdlog::logger> logger{};
#endif

  std::string name_;
  std::mutex mu_;
  Level level_{Level::kDisable};

public:
  explicit Logger(const char *name) : name_(name) {
#ifdef USE_SPDLOG
    logger = spdlog::stdout_color_mt(name);
#endif

    SetLevel(level_);
  }

  void SetLevel(Level level) {
    level_ = level;
#ifdef USE_SPDLOG
    {
      auto loglevel = [](Level level) {
        switch (level) {
        case Level::kTrace:
          return spdlog::level::trace;
        case Level::kInfo:
          return spdlog::level::info;
        case Level::kWarn:
          return spdlog::level::warn;
        case Level::kError:
          return spdlog::level::err;
        case Level::kDisable:
          return spdlog::level::off;
        }
        std::terminate();
      };
      logger->set_level(loglevel(level));
    }
#endif
  }

  template <typename... Args> void Info(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->info(fmt, std::forward<Args>(args)...);
#endif

#ifdef USE_STDLOG
    if (Compare(level_, Level::kInfo) > 0) {
      return;
    }
    static std::string level = "INFO";
    Timestamp now = Timestamp::Now();
    std::string msg = fmt::format(fmt, std::forward<Args>(args)...);
    fmt::print("[{}] [{}] [{}] {}\n", now, name_, level, msg);
#endif
  }

  template <typename... Args> void Warn(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->warn(fmt, std::forward<Args>(args)...);
#endif

#ifdef USE_STDLOG
    if (Compare(level_, Level::kWarn) > 0) {
      return;
    }
    static std::string level = "WARN";
    Timestamp now = Timestamp::Now();
    std::string msg = fmt::format(fmt, std::forward<Args>(args)...);
    fmt::print("[{}] [{}] [{}] {}\n", now, name_, level, msg);
#endif
  }

  template <typename... Args> void Error(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->error(fmt, std::forward<Args>(args)...);
#endif

#ifdef USE_STDLOG
    if (Compare(level_, Level::kError) > 0) {
      return;
    }
    static std::string level = "ERROR";
    Timestamp now = Timestamp::Now();
    std::string msg = fmt::format(fmt, std::forward<Args>(args)...);
    fmt::print("[{}] [{}] [{}] {}\n", now, name_, level, msg);
#endif
  }

  template <typename... Args> void Trace(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->trace(fmt, std::forward<Args>(args)...);
#endif

#ifdef USE_STDLOG
    if (Compare(level_, Level::kTrace) > 0) {
      return;
    }
    static std::string level = "TRACE";
    Timestamp now = Timestamp::Now();
    std::string msg = fmt::format(fmt, std::forward<Args>(args)...);
    fmt::print("[{}] [{}] [{}] {}\n", now, name_, level, msg);
#endif
  }

  template <typename... Args> void Fatal(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->critical(fmt, std::forward<Args>(args)...);
#endif

#ifdef USE_STDLOG
    if (Compare(level_, Level::kDisable) > 0) {
      return;
    }
    static std::string level = "FATAL";
    Timestamp now = Timestamp::Now();
    std::string msg = fmt::format(fmt, std::forward<Args>(args)...);
    fmt::print("[{}] [{}] [{}] {}\n", now, name_, level, msg);
#endif
    std::terminate();
  }
};
} // namespace pedrolib

#endif // PEDROLIB_LOGGER_LOGGER_H
