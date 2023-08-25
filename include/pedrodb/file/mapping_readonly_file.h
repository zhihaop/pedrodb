#ifndef PEDRODB_FILE_MAPPING_READONLY_FILE_H
#define PEDRODB_FILE_MAPPING_READONLY_FILE_H

#include <sys/mman.h>
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/status.h"

namespace pedrodb {
class MappingReadonlyFile final : public ReadableFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<MappingReadonlyFile>;

 private:
  mutable File file_{};
  const char* data_{};
  size_t capacity_;

 public:
  [[nodiscard]] ReadableBuffer GetReadonlyBuffer() const noexcept {
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

  Status Open(const std::string& path) override {
    File::OpenOption option;
    option.mode = File::OpenMode::kRead;

    file_ = File::Open(path.c_str(), option);
    if (!file_.Valid()) {
      return Status::kIOError;
    }

    capacity_ = file_.GetSize();
    if (capacity_ < 0) {
      PEDRODB_ERROR("failed to get size of ptr {}: {}", path, file_.GetError());
      return Status::kIOError;
    }

    data_ = (const char*)::mmap(nullptr, capacity_, PROT_READ, MAP_PRIVATE,
                                file_.Descriptor(), 0);
    if (data_ == (char*)-1) {
      PEDRODB_ERROR("failed to mmap file {}: {}", file_, Error{errno});
      return Status::kIOError;
    }

    if (::madvise((void*)data_, capacity_, MADV_RANDOM)) {
      PEDRODB_ERROR("failed to advice mmap file {}: {}", file_, Error{errno});
      return Status::kIOError;
    }
    return Status::kOk;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_MAPPING_READONLY_FILE_H
