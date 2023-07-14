#ifndef PEDRONET_LOGGER_LOGGER_H
#define PEDRONET_LOGGER_LOGGER_H

#include <pedrolib/logger/logger.h>

namespace pedronet::logger {
using Level = pedrolib::Logger::Level;

pedrolib::Logger& GetLogger();

void SetLevel(Level level);
}  // namespace pedronet::logger

#define PEDRONET_TRACE(fmt, args...) \
  pedronet::logger::GetLogger().Trace(fmt, ##args)
#define PEDRONET_INFO(fmt, args...) \
  pedronet::logger::GetLogger().Info(fmt, ##args)
#define PEDRONET_WARN(fmt, args...) \
  pedronet::logger::GetLogger().Warn(fmt, ##args)
#define PEDRONET_ERROR(fmt, args...) \
  pedronet::logger::GetLogger().Error(fmt, ##args)
#define PEDRONET_FATAL(fmt, args...) \
  pedronet::logger::GetLogger().Fatal(fmt, ##args)

#endif  // PEDRONET_LOGGER_LOGGER_H
