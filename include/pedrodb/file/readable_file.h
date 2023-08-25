#ifndef PEDRODB_FILE_READABLE_FILE_H
#define PEDRODB_FILE_READABLE_FILE_H
#include <pedrolib/noncopyable.h>
#include <memory>
namespace pedrodb {

struct ReadableFile {
  using Ptr = std::shared_ptr<ReadableFile>;

  ReadableFile() = default;
  virtual ~ReadableFile() = default;
  [[nodiscard]] virtual uint64_t Size() const noexcept = 0;
  [[nodiscard]] virtual Error GetError() const noexcept = 0;
  virtual ssize_t Read(uint64_t offset, char* buf, size_t n) = 0;
  virtual Status Open(const std::string& path) = 0;
};

class ReadableBuffer {
  const char* data_{};
  size_t read_index_{};
  const size_t capacity_{};

 public:
  ReadableBuffer(const char* data, size_t capacity)
      : data_(data), capacity_(capacity) {}
  [[nodiscard]] const char* ReadIndex() const noexcept {
    return data_ + read_index_;
  }

  void SetReadOffset(size_t offset) noexcept { read_index_ = offset; }

  [[nodiscard]] size_t GetReadOffset() const noexcept { return read_index_; }

  [[nodiscard]] size_t ReadableBytes() const noexcept {
    return capacity_ - read_index_;
  }

  void Retrieve(size_t n) noexcept { read_index_ += n; }
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_READABLE_FILE_H
