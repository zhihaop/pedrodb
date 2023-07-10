#include "pedrokv/logger/logger.h"

namespace pedrokv::logger {
pedrolib::Logger &GetLogger() {
  static auto logger = pedrolib::Logger("pedrokv");
  return logger;
}
void SetLevel(Level level) { GetLogger().SetLevel(level); }
} // namespace pedrokv::logger