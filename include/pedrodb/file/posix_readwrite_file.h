#ifndef PEDRODB_FILE_POSIX_READWRITE_FILE_H
#define PEDRODB_FILE_POSIX_READWRITE_FILE_H
#include "pedrodb/file/readable_file.h"
#include "pedrodb/file/readwrite_file.h"

namespace pedrodb {
class PosixReadWriteFile final : public ReadWriteFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<PosixReadWriteFile>;

  [[nodiscard]] uint64_t Size() const noexcept override {
    return buffer_.size();
  }

  [[nodiscard]] Error GetError() const noexcept override {
    return file_.GetError();
  }

  [[nodiscard]] ReadableBuffer GetReadableBuffer() const noexcept override {
    return {buffer_.data(), buffer_.size()};
  }

  ssize_t Read(uint64_t offset, char* buf, size_t n) override {
    if (offset >= offset_) {
      return -1;
    }

    size_t end = offset + n;
    end = std::min(end, offset_);
    memcpy(buf, buffer_.data() + offset, end - offset);
    return static_cast<ssize_t>(end - offset);
  }

  void SetWriteOffset(size_t n) noexcept override { offset_ = n; }

  WritableBuffer Allocate(size_t n) override {
    if (n > buffer_.size() - offset_) {
      return {nullptr, 0, (size_t)-1};
    }

    WritableBuffer buffer{buffer_.data() + offset_, n, offset_};
    offset_ += n;
    return buffer;
  }

  Error Flush(bool force) override {
    bool flush = force;
    if (offset_ - flush_offset_ > kBlockSize) {
      flush = true;
    }

    if (flush && offset_ - flush_offset_ > 0) {
      if (file_.Write(buffer_.data() + flush_offset_,
                      offset_ - flush_offset_) != offset_ - flush_offset_) {
        return file_.GetError();
      }
      flush_offset_ = offset_;
    }
    return Error::kOk;
  }

  Error Sync() override {
    if (auto err = Flush(true); err != Error::kOk) {
      return err;
    }
    return file_.Sync();
  }

  Status Open(const std::string& path) override { return Open(path, -1); }

  Status Open(const std::string& path, size_t capacity) override {
    File::OpenOption option;
    option.create = 0777;
    option.mode = File::OpenMode::kReadWrite;

    file_ = File::Open(path.c_str(), option);
    if (!file_.Valid()) {
      PEDRODB_ERROR("failed to open active file {}: {}", path,
                    file_.GetError());
      return Status::kIOError;
    }

    size_t length = file_.GetSize();
    if (length == -1) {
      PEDRODB_ERROR("failed to get size of file {}", file_.GetError());
      return Status::kIOError;
    }

    if (capacity != -1 && length == 0) {
      if (auto err = file_.Reserve(capacity); err != Error::kOk) {
        PEDRODB_ERROR("failed to resize file {}", file_.GetError());
        return Status::kIOError;
      }
      length = capacity;
      buffer_.resize(length);
      return Status::kOk;
    }

    if (capacity != -1 && length != capacity) {
      PEDRODB_ERROR("Format error: file size is not correct");
      return Status::kCorruption;
    }

    buffer_.resize(length);
    if (file_.Pread(0, buffer_.data(), buffer_.size()) != buffer_.size()) {
      return Status::kIOError;
    }
    return Status::kOk;
  }

  std::unique_lock<std::mutex> GetLock() noexcept override {
    return std::unique_lock{mu_};
  }

 private:
  File file_;

  std::string buffer_;
  size_t flush_offset_{};
  size_t offset_{};
  std::mutex mu_;
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_POSIX_READWRITE_FILE_H
