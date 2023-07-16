#ifndef PEDRODB_DEFINES_H
#define PEDRODB_DEFINES_H
#include <pedrolib/executor/thread_pool_executor.h>

#include <pedrolib/buffer/array_buffer.h>
#include <pedrolib/buffer/buffer.h>

#include <pedrolib/duration.h>
#include <pedrolib/file/error.h>
#include <pedrolib/file/file.h>
#include <pedrolib/noncopyable.h>
#include <pedrolib/nonmovable.h>
#include <pedrolib/timestamp.h>

namespace pedrodb {
using pedrolib::ArrayBuffer;

using pedrolib::Executor;
using DefaultExecutor = pedrolib::ThreadPoolExecutor;

using pedrolib::Duration;
using pedrolib::Error;
using pedrolib::File;
using pedrolib::Timestamp;

using pedrolib::AppendInt;
using pedrolib::noncopyable;
using pedrolib::nonmovable;
using pedrolib::PeekInt;
using pedrolib::RetrieveInt;

using file_id_t = uint32_t;

// the maximum size of file is 32MB.
const uint64_t kMaxFileBytes = 128ULL << 20;

// the erase block size of SSD is 512KB.
const uint32_t kBlockSize = 512 << 10;

// the page size of SSD is 4KB.
const uint32_t kPageSize = 4 << 10;

inline static uint32_t Hash(std::string_view s) {
  return std::hash<std::string_view>()(s);
}

}  // namespace pedrodb

#endif  // PEDRODB_DEFINES_H
