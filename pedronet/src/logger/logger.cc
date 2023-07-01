#include "pedronet/logger/logger.h"

#ifdef USE_SPDLOG

#include <spdlog/sinks/stdout_color_sinks.h>
spdlog::logger &pedronet::logger::GetLogger() {
  static auto logger = spdlog::stdout_color_mt("pedronet");
  return *logger;
}
#endif
