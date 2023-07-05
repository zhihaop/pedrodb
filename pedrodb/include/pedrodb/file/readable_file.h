#ifndef PEDRODB_FILE_READABLE_FILE_H
#define PEDRODB_FILE_READABLE_FILE_H

#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/status.h"

namespace pedrodb {
class ReadableFile : noncopyable {
  uint64_t size_{};
  File file_{};

public:
  ReadableFile() = default;
  ~ReadableFile() = default;
  ReadableFile(ReadableFile &&other) noexcept
      : size_(other.size_), file_(std::move(other.file_)) {}

  ReadableFile &operator=(ReadableFile &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    size_ = other.size_;
    file_ = std::move(other.file_);
    other.size_ = 0;
    return *this;
  }

  ssize_t Read(uint64_t offset, char *data, size_t length) {
    return file_.Pread(offset, data, length);
  }

  ssize_t Readv(uint64_t offset, std::string_view *io, size_t n) {
    return file_.Preadv(offset, io, n);
  }

  uint64_t Size() const noexcept { return size_; }

  Error GetError() { return file_.GetError(); }

  static Status Open(const std::string &filename, ReadableFile *file) {
    File::OpenOption option{.mode = File ::OpenMode::kRead};
    auto f = File::Open(filename.c_str(), option);
    if (!f.Valid()) {
      PEDRODB_ERROR("failed to create active file {}: {}", filename,
                    f.GetError());
      return Status::kIOError;
    }

    int64_t size = File::Size(f);
    if (size < 0) {
      PEDRODB_ERROR("failed to seek tail {}: {}", filename, f.GetError());
      return Status::kIOError;
    }

    file->size_ = size;
    file->file_ = std::move(f);
    return Status::kOk;
  }
};
} // namespace pedrodb

#endif // PEDRODB_FILE_READABLE_FILE_H
