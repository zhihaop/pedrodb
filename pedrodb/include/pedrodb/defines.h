#ifndef PEDRODB_DEFINES_H
#define PEDRODB_DEFINES_H
#include <pedrolib/executor/thread_pool_executor.h>

#include <pedrolib/buffer/array_buffer.h>
#include <pedrolib/buffer/buffer.h>
#include <pedrolib/buffer/buffer_slice.h>
#include <pedrolib/buffer/buffer_view.h>

#include <pedrolib/duration.h>
#include <pedrolib/file/error.h>
#include <pedrolib/file/file.h>
#include <pedrolib/noncopyable.h>
#include <pedrolib/nonmovable.h>
#include <pedrolib/timestamp.h>

namespace pedrodb {
using ArrayBuffer = pedrolib::ArrayBuffer;
using Buffer = pedrolib::Buffer;
using BufferView = pedrolib::BufferView;
using BufferSlice = pedrolib::BufferSlice;

using Executor = pedrolib::Executor;
using DefaultExecutor = pedrolib::ThreadPoolExecutor;

using Timestamp = pedrolib::Timestamp;
using Duration = pedrolib::Duration;
using File = pedrolib::File;
using Error = pedrolib::Error;

using nonmovable = pedrolib::nonmovable;
using noncopyable = pedrolib::noncopyable;

// the maximum size of file is 64MB.
const uint64_t kMaxFileBytes = 64ULL << 20;

const uint32_t kBatchVersions = 128 << 10;

// the erase block size of SSD is 512KB.
const uint32_t kBlockSize = 512 << 10;

// the page size of SSD is 4KB.
const uint32_t kPageSize = 4 << 10;

struct ValueLocation : public pedrolib::Comparable<ValueLocation> {
  uint32_t id{};
  uint32_t offset{};

  ValueLocation() = default;
  ValueLocation(const ValueLocation &) = default;
  ValueLocation &operator=(const ValueLocation &) = default;

  static int Compare(const ValueLocation &x, const ValueLocation &y) noexcept {
    if (x.id != y.id) {
      return x.id < y.id ? -1 : 1;
    }
    if (x.offset != y.offset) {
      return x.offset < y.offset ? -1 : 1;
    }
    return 0;
  }
};

struct ValueLocationHash {
  size_t operator()(const ValueLocation &v) const noexcept {
    return std::hash<uint64_t>()(*reinterpret_cast<const uint64_t *>(&v));
  }
};

} // namespace pedrodb

#endif // PEDRODB_DEFINES_H
