#ifndef PEDROLIB_LOGGER_LOGGER_H
#define PEDROLIB_LOGGER_LOGGER_H

#define USE_SPDLOG

#ifdef USE_SPDLOG
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#endif

namespace pedrolib {

class Logger {

#ifdef USE_SPDLOG
  std::shared_ptr<spdlog::logger> logger{};
#endif

  std::string name_;

public:
  explicit Logger(const char *name) : name_(name) {
#ifdef USE_SPDLOG
    logger = spdlog::stdout_color_mt(name);
#endif
  }

  enum class Level {
    kTrace,
    kInfo,
    kWarn,
    kError,
    kDisable,
  };

  void SetLevel(Level level) {
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
  }

  template <typename... Args> void Warn(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->warn(fmt, std::forward<Args>(args)...);
#endif
  }

  template <typename... Args> void Error(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->error(fmt, std::forward<Args>(args)...);
#endif
  }

  template <typename... Args> void Trace(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->trace(fmt, std::forward<Args>(args)...);
#endif
  }

  template <typename... Args> void Fatal(const char *fmt, Args &&...args) {
#ifdef USE_SPDLOG
    logger->critical(fmt, std::forward<Args>(args)...);
#endif
    std::terminate();
  }
};
} // namespace pedrolib

#endif // PEDROLIB_LOGGER_LOGGER_H
