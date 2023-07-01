#ifndef PEDRONET_LOGGER_LOGGER_H
#define PEDRONET_LOGGER_LOGGER_H

#define USE_SPDLOG

#ifdef USE_SPDLOG
#include <spdlog/spdlog.h>

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
