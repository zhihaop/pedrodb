#ifndef PEDRODB_OPTIONS_H
#define PEDRODB_OPTIONS_H

#include "pedrodb/defines.h"
#include <string>

namespace pedrodb {
struct Options {
  int16_t max_open_files = 16;
  size_t read_cache_bytes = kMaxFileBytes;
  size_t compaction_threshold_bytes = kMaxFileBytes / 2;
  Duration sync_interval = Duration::Seconds(10);
  size_t executor_threads = 1;
  bool prefetch = false;
};

struct ReadOptions {};
struct WriteOptions {
  bool sync = false;
};
} // namespace pedrodb

#endif // PEDRODB_OPTIONS_H
