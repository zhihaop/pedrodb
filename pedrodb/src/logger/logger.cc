#include "pedrodb/logger/logger.h"

namespace pedrodb::logger {
pedrolib::Logger &GetLogger() {
  static pedrolib::Logger logger("pedrodb");
  return logger;
}
void SetLevel(Level level) { GetLogger().SetLevel(level); }
} // namespace pedrodb::logger