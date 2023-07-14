#include "pedronet/logger/logger.h"

namespace pedronet::logger {
pedrolib::Logger& GetLogger() {
  static auto logger = pedrolib::Logger("pedronet");
  return logger;
}
void SetLevel(Level level) {
  GetLogger().SetLevel(level);
}
}  // namespace pedronet::logger
