#include "pedrodb/db.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
Status DB::Open(const Options &options, const std::string &name, DB **db) {
  PEDRODB_INFO("Open database {}", name);
  return Status::kCorruption;
}
} // namespace pedrodb