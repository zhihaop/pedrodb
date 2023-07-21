#ifndef PEDRODB_FILE_READONLY_FILE_H
#define PEDRODB_FILE_READONLY_FILE_H

#include <sys/mman.h>
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/status.h"

namespace pedrodb {
class PosixReadonlyFile : public ReadableFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<PosixReadonlyFile>;

 private:
  mutable File file_{};

  const size_t capacity_;

 public:
  explicit PosixReadonlyFile(size_t capacity, File file)
      : capacity_(capacity), file_(std::move(file)) {}

  ~PosixReadonlyFile() override = default;

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

    *ptr = std::make_shared<PosixReadonlyFile>(size, std::move(file));
    return Status::kOk;
  }
};

class MappingReadonlyFile : public ReadableFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<PosixReadonlyFile>;

 private:
  mutable File file_{};
  const char* data_{};
  const size_t capacity_;

 public:
  explicit MappingReadonlyFile(size_t capacity, File file)
      : capacity_(capacity), file_(std::move(file)) {
    if (!file_.Valid() || capacity_ == 0) {
      return;
    }

    data_ = (const char*)::mmap(nullptr, capacity_, PROT_READ, MAP_PRIVATE,
                                file_.Descriptor(), 0);
    if (data_ == (char*)-1) {
      PEDRODB_ERROR("failed to mmap file {}: {}", file_, Error{errno});
      data_ = nullptr;
      return;
    }

    if (::madvise((void*)data_, capacity_, MADV_RANDOM)) {
      PEDRODB_ERROR("failed to advice mmap file {}: {}", file_, Error{errno});
      data_ = nullptr;
      return;
    }
  }
  
  [[nodiscard]] ReadonlyBuffer GetReadonlyBuffer() const noexcept {
    return {data_, capacity_};
  }
  

  ~MappingReadonlyFile() override {
    if (data_ != nullptr) {
      if (::madvise((void*)data_, capacity_, MADV_DONTNEED)) {
        PEDRODB_ERROR("failed to advice mmap file {}: {}", file_, Error{errno});
        return;
      }
      
      if (::munmap((void*)data_, capacity_)) {
        PEDRODB_ERROR("failed to unmap file {}: {}", file_, Error{errno});
        return;
      }
    }
  }

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

    *ptr = std::make_shared<PosixReadonlyFile>(size, std::move(file));
    return Status::kOk;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_READONLY_FILE_H
