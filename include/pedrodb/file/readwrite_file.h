#ifndef PEDRODB_FILE_READWRITE_FILE_H
#define PEDRODB_FILE_READWRITE_FILE_H
#include "pedrodb/defines.h"

namespace pedrodb {

class WritableBuffer {
  char* data_{};
  size_t write_index_{};
  const size_t capacity_{};

  size_t offset_;

 public:
  WritableBuffer(char* data, size_t capacity, size_t offset)
      : data_(data), capacity_(capacity), offset_(offset) {}

  [[nodiscard]] char* WriteIndex() const noexcept {
    return data_ + write_index_;
  }

  [[nodiscard]] size_t WritableBytes() const noexcept {
    return capacity_ - write_index_;
  }

  void Append(const char* buf, size_t n) noexcept {
    memcpy(data_ + write_index_, buf, n);
    write_index_ += n;
  }

  void Append(size_t n) noexcept { write_index_ += n; }

  [[nodiscard]] size_t GetOffset() const noexcept { return offset_; }
};

struct ReadWriteFile : public ReadableFile {
  using Ptr = std::shared_ptr<ReadWriteFile>;

  ReadWriteFile() = default;
  virtual ~ReadWriteFile() = default;

  [[nodiscard]] uint64_t Size() const noexcept override = 0;

  virtual Error Flush(bool force) = 0;
  virtual Error Sync() = 0;
  [[nodiscard]] Error GetError() const noexcept override = 0;

  virtual Status Open(const std::string& path, size_t capacity) = 0;
  Status Open(const std::string& path) override = 0;

  virtual WritableBuffer Allocate(size_t n) = 0;
  ssize_t Read(uint64_t offset, char* buf, size_t n) override = 0;

  [[nodiscard]] virtual ReadableBuffer GetReadableBuffer() const noexcept = 0;
  virtual void SetWriteOffset(size_t offset) noexcept = 0;

  [[nodiscard]] virtual std::unique_lock<std::mutex> GetLock() noexcept = 0;
};
}  // namespace pedrodb

#endif  //PEDRODB_FILE_READWRITE_FILE_H
