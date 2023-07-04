#include "pedrodb/status.h"

namespace pedrodb {
const Status Status::kOk{0};
const Status Status::kNotFound{1};
const Status Status::kCorruption{2};
const Status Status::kNotSupported{3};
const Status Status::kInvalidArgument{4};
const Status Status::kIOError{5};
} // namespace pedrodb