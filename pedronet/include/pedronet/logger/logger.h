#ifndef PEDRONET_LOGGER_LOGGER_H
#define PEDRONET_LOGGER_LOGGER_H

#define USE_SPDLOG

#ifdef USE_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace pedronet::logger {

enum class Level {
  kTrace,
  kInfo,
  kWarn,
  kError,
  kDisable,
};

inline static void SetLevel(Level level) {
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
    spdlog::set_level(loglevel(level));
  }
#endif
}
} // namespace pedronet::logger

#ifdef USE_SPDLOG

#define PEDRONET_TRACE(fmt, args...) spdlog::trace(fmt, ##args)
#define PEDRONET_INFO(fmt, args...) spdlog::info(fmt, ##args)
#define PEDRONET_WARN(fmt, args...) spdlog::warn(fmt, ##args)
#define PEDRONET_ERROR(fmt, args...) spdlog::error(fmt, ##args)
#else
#define PEDRONET_TRACE(fmt, args...)
#define PEDRONET_INFO(fmt, args...)
#define PEDRONET_WARN(fmt, args...)
#define PEDRONET_ERROR(fmt, args...)
#endif

#endif // PEDRONET_LOGGER_LOGGER_H
