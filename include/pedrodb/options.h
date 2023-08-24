#ifndef PEDRODB_OPTIONS_H
#define PEDRODB_OPTIONS_H

#include <pedrolib/executor/thread_pool_executor.h>
#include <string>
#include "pedrodb/defines.h"

namespace pedrodb {

struct ReadCacheOptions {
  bool enable{true};
  size_t read_cache_bytes{32 << 20};
  size_t segments{std::thread::hardware_concurrency()};
};

struct Options {
  uint8_t max_open_files{16};

  struct {
    size_t threshold_bytes{static_cast<size_t>(kMaxFileBytes * 0.75)};
    Duration interval{Duration::Seconds(5)};
  } compaction{};

  bool compress_value{true};
  Duration sync_interval{Duration::Seconds(10)};
  int32_t sync_max_io_error{32};

  ReadCacheOptions read_cache{};

  std::shared_ptr<Executor> executor{std::make_shared<DefaultExecutor>(1)};
};

struct ReadOptions {
  bool use_read_cache{true};
};

struct WriteOptions {
  bool sync{false};
};
}  // namespace pedrodb

#endif  // PEDRODB_OPTIONS_H
