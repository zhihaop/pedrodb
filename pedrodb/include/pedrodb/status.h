#ifndef PEDRODB_STATUS_H
#define PEDRODB_STATUS_H

#include <string_view>

namespace pedrodb {
enum class Status {
  kOk = 0,
  kNotFound = 1,
  kCorruption = 2,
  kNotSupported = 3,
  kInvalidArgument = 4,
  kIOError = 5
};
} // namespace pedrodb

#endif // PEDRODB_STATUS_H
