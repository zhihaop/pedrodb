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

using file_t = uint32_t;

// the maximum size of file is 32MB.
const uint64_t kMaxFileBytes = 32ULL << 20;

const uint32_t kBatchVersions = 128 << 10;

// the erase block size of SSD is 512KB.
const uint32_t kBlockSize = 512 << 10;

// the page size of SSD is 4KB.
const uint32_t kPageSize = 4 << 10;

} // namespace pedrodb

#endif // PEDRODB_DEFINES_H
