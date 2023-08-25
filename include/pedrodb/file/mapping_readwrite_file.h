#ifndef PEDRODB_FILE_MAPPING_READWRITE_FILE_H
#define PEDRODB_FILE_MAPPING_READWRITE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/file/readwrite_file.h"
#include "pedrodb/logger/logger.h"

#include <sys/mman.h>
namespace pedrodb {

class MappingReadWriteFile final : public ReadWriteFile,
                                   noncopyable,
                                   nonmovable {
 public:
  using Ptr = std::shared_ptr<MappingReadWriteFile>;

  ~MappingReadWriteFile() override {
    if (data_ != nullptr) {
      munmap(data_, length_);
      madvise(data_, length_, MADV_DONTNEED);
      data_ = nullptr;
    }
  }

  [[nodiscard]] ReadableBuffer GetReadableBuffer() const noexcept override {
    return {data_, length_};
  }

  void SetWriteOffset(size_t offset) noexcept override {
    write_index_ = offset;
  }

  Status Open(const std::string& path) override { return Open(path, -1); }

  Status Open(const std::string& path, size_t capacity) override {
    File::OpenOption option;
    option.create = 0777;
    option.mode = File::OpenMode::kReadWrite;

    file_ = File::Open(path.c_str(), option);
    if (!file_.Valid()) {
      PEDRODB_ERROR("failed to open active file {}: {}", path, Error{errno});
      return Status::kIOError;
    }

    length_ = file_.GetSize();
    if (length_ == -1) {
      PEDRODB_ERROR("failed to get size of file {}", Error{errno});
      return Status::kIOError;
    }

    if (capacity != -1 && length_ == 0) {
      if (auto err = file_.Reserve(capacity); err != Error::kOk) {
        PEDRODB_ERROR("failed to resize file {}", file_.GetError());
        return Status::kIOError;
      }
      length_ = capacity;
    }

    if (capacity != -1 && length_ != capacity) {
      PEDRODB_ERROR("Format error: file size is not correct");
      return Status::kCorruption;
    }

    data_ = (char*)::mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_SHARED,
                          file_.Descriptor(), 0);
    if (data_ == (char*)(-1)) {
      PEDRODB_ERROR("failed to mmap file {}", file_.GetError());
      return Status::kIOError;
    }
    return Status::kOk;
  }

  [[nodiscard]] uint64_t Size() const noexcept override { return length_; }

  [[nodiscard]] Error GetError() const noexcept override {
    return file_.GetError();
  }

  ssize_t Read(uint64_t offset, char* buf, size_t n) override {
    memcpy(buf, data_ + offset, n);
    return static_cast<ssize_t>(n);
  }

  Error Flush(bool force) override { return Error::kOk; }

  WritableBuffer Allocate(size_t n) override {
    if (n > length_ - write_index_) {
      return {nullptr, 0, (size_t)-1};
    }

    WritableBuffer buffer{data_ + write_index_, n, write_index_};
    write_index_ += n;
    return buffer;
  }

  Error Sync() override {
    if (msync(data_, length_, MS_SYNC) < 0) {
      return file_.GetError();
    }
    return Error::kOk;
  }

  std::unique_lock<std::mutex> GetLock() noexcept override {
    return std::unique_lock{mu_};
  }

 private:
  File file_;
  char* data_{};
  size_t write_index_{};
  size_t length_{};
  std::mutex mu_;
};

}  // namespace pedrodb
#endif  // PEDRODB_FILE_MAPPING_READWRITE_FILE_H
