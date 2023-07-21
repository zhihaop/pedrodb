#ifndef PEDRODB_OPTIONS_H
#define PEDRODB_OPTIONS_H

#include <pedrolib/executor/thread_pool_executor.h>
#include <string>
#include "pedrodb/defines.h"

namespace pedrodb {
struct Options {
  int8_t max_open_files = 16;

  struct {
    size_t threshold_bytes = kMaxFileBytes * 0.75;
    Duration interval = Duration::Seconds(5);
  } compaction{};
  
  bool compress_value = true;
  Duration sync_interval = Duration::Seconds(10);

  std::shared_ptr<Executor> executor{std::make_shared<DefaultExecutor>(1)};
};

struct ReadOptions {};

struct WriteOptions {
  bool sync = false;
};
}  // namespace pedrodb

#endif  // PEDRODB_OPTIONS_H
