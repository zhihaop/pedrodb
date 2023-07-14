#ifndef PEDROKV_LOGGER_LOGGER_H
#define PEDROKV_LOGGER_LOGGER_H

#include <pedrolib/logger/logger.h>

namespace pedrokv::logger {
using Level = pedrolib::Logger::Level;

pedrolib::Logger& GetLogger();
void SetLevel(Level level);

}  // namespace pedrokv::logger

#define PEDROKV_TRACE(fmt, args...) \
  pedrokv::logger::GetLogger().Trace(fmt, ##args)
#define PEDROKV_INFO(fmt, args...) \
  pedrokv::logger::GetLogger().Info(fmt, ##args)
#define PEDROKV_WARN(fmt, args...) \
  pedrokv::logger::GetLogger().Warn(fmt, ##args)
#define PEDROKV_ERROR(fmt, args...) \
  pedrokv::logger::GetLogger().Error(fmt, ##args)
#define PEDROKV_FATAL(fmt, args...) \
  pedrokv::logger::GetLogger().Fatal(fmt, ##args)

#endif  // PEDROKV_LOGGER_LOGGER_H