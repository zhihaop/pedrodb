#ifndef PEDRONET_LOGGER_LOGGER_H
#define PEDRONET_LOGGER_LOGGER_H

#define USE_SPDLOG

#ifdef USE_SPDLOG
#include <spdlog/spdlog.h>

namespace pedronet::logger {
spdlog::logger &GetLogger();
}
#define PEDRONET_TRACE(fmt, args...)                                           \
  pedronet::logger::GetLogger().trace(fmt, ##args)
#define PEDRONET_INFO(fmt, args...)                                            \
  pedronet::logger::GetLogger().info(fmt, ##args)
#define PEDRONET_WARN(fmt, args...)                                            \
  pedronet::logger::GetLogger().warn(fmt, ##args)
#define PEDRONET_ERROR(fmt, args...)                                           \
  pedronet::logger::GetLogger().error(fmt, ##args)

#else
#define PEDRONET_TRACE(fmt, args...)
#define PEDRONET_INFO(fmt, args...)
#define PEDRONET_WARN(fmt, args...)
#define PEDRONET_ERROR(fmt, args...)
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
    GetLogger().set_level(loglevel(level));
  }
#endif
}
} // namespace pedronet::logger

#endif // PEDRONET_LOGGER_LOGGER_H
