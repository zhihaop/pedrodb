#ifndef PEDRODB_LOGGER_LOGGER_H
#define PEDRODB_LOGGER_LOGGER_H

#include <pedrolib/logger/logger.h>

namespace pedrodb::logger {
using Level = pedrolib::Logger::Level;

pedrolib::Logger& GetLogger();
void SetLevel(Level level);

}  // namespace pedrodb::logger

#define PEDRODB_TRACE(fmt, args...) \
  pedrodb::logger::GetLogger().Trace(fmt, ##args)
#define PEDRODB_INFO(fmt, args...) \
  pedrodb::logger::GetLogger().Info(fmt, ##args)
#define PEDRODB_WARN(fmt, args...) \
  pedrodb::logger::GetLogger().Warn(fmt, ##args)
#define PEDRODB_ERROR(fmt, args...) \
  pedrodb::logger::GetLogger().Error(fmt, ##args)
#define PEDRODB_FATAL(fmt, args...) \
  pedrodb::logger::GetLogger().Fatal(fmt, ##args)

template <typename T>
bool StatusOk(T stat) {
  return stat == T::kOk;
}

#define PEDRODB_IGNORE_ERROR(err)             \
  do {                                        \
    if (!StatusOk(err)) {                     \
      PEDRODB_ERROR("ignore error: {}", err); \
    }                                         \
  } while (0)

#endif  // PEDRODB_LOGGER_LOGGER_H
