#ifndef PEDRODB_FILE_READONLY_FILE_H
#define PEDRODB_FILE_READONLY_FILE_H

#include <sys/mman.h>
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/status.h"

namespace pedrodb {
class ReadonlyFile : public ReadableFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<ReadonlyFile>;

 private:
  mutable File file_{};

  const size_t capacity_;

 public:
  explicit ReadonlyFile(size_t capacity, File file)
      : capacity_(capacity), file_(std::move(file)) {}

  ~ReadonlyFile() override = default;

  ssize_t Read(uint64_t offset, char* data, size_t length) override {
    return file_.Pread(offset, data, length);
  }

  uint64_t Size() const noexcept override { return capacity_; }

  Error GetError() const noexcept override { return file_.GetError(); }

  static Status Open(const std::string& path, ReadableFile::Ptr* ptr) {
    File::OpenOption option;
    option.mode = File::OpenMode::kRead;

    auto file = File::Open(path.c_str(), option);
    if (!file.Valid()) {
      return Status::kIOError;
    }

    int64_t size = File::Size(file);
    if (size < 0) {
      PEDRODB_ERROR("failed to get size of ptr {}: {}", path, file.GetError());
      return Status::kIOError;
    }

    *ptr = std::make_shared<ReadonlyFile>(size, std::move(file));
    return Status::kOk;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_READONLY_FILE_H
