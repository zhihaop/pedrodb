#ifndef PEDRODB_DEFINES_H
#define PEDRODB_DEFINES_H
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

using Timestamp = pedrolib::Timestamp;
using Duration = pedrolib::Duration;
using File = pedrolib::File;
using Error = pedrolib::Error;

using nonmovable = pedrolib::nonmovable;
using noncopyable = pedrolib::noncopyable;

// the maximum size of file is 128MB.
const uint64_t kMaxFileBytes = 128ULL << 20;

const uint32_t kBatchVersions = 16 << 10;

struct ValueLocation {
  uint32_t id{};
  uint32_t offset{};

  ValueLocation() = default;
  ValueLocation(const ValueLocation &) = default;
  ValueLocation &operator=(const ValueLocation &) = default;

  bool operator==(const ValueLocation &other) const noexcept {
    return id == other.id && offset == other.offset;
  }
};

struct ValueLocationHash {
  size_t operator()(const ValueLocation &v) const noexcept {
    return std::hash<uint64_t>()(*reinterpret_cast<const uint64_t *>(&v));
  }
};

} // namespace pedrodb

#endif // PEDRODB_DEFINES_H
