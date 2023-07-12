#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
class WritableFile final : public ReadableFile {
  File file_{};
  std::string buf_;
  size_t offset_{};
  size_t capacity_{};
  file_t id_{};
  mutable std::mutex mu_;

  WritableFile() = default;

public:
  ~WritableFile() override = default;

  static Status Open(const std::string &filename, file_t id, size_t capacity,
                     WritableFile **f) {
    File::OpenOption option{.mode = File ::OpenMode::kReadWrite,
                            .create = 0777};
    auto file = File::Open(filename.c_str(), option);
    if (!file.Valid()) {
      PEDRODB_ERROR("failed to create active file {}: {}", filename,
                    file.GetError());
      return Status::kIOError;
    }

    int64_t i64_size = File::Size(file);
    auto size = static_cast<size_t>(i64_size);
    if (i64_size < 0 || i64_size != size || size > capacity) {
      PEDRODB_ERROR("failed to get size of file {}", filename);
      return Status::kIOError;
    }

    std::string buf(size, 0);
    if (file.Read(buf.data(), buf.size()) != size) {
      PEDRODB_ERROR("failed to read file {}", filename);
      return Status::kIOError;
    }

    auto ptr = *f = new WritableFile();
    ptr->file_ = std::move(file);
    ptr->offset_ = buf.size();
    ptr->buf_ = std::move(buf);
    ptr->capacity_ = capacity;
    ptr->id_ = id;
    
    ptr->buf_.reserve(capacity);
    return Status::kOk;
  }

  uint64_t Size() const noexcept override {
    std::unique_lock lock{mu_};
    return buf_.size();
  }

  Error GetError() const noexcept override { return file_.GetError(); }

  file_t GetFile() const noexcept { return id_; }

  ssize_t Read(uint64_t offset, char *buf, size_t n) override {
    std::unique_lock lock{mu_};
    memcpy(buf, buf_.data() + offset, n);
    return static_cast<ssize_t>(n);
  }

  Error Flush(bool force) {
    std::string_view buf;
    {
      std::unique_lock lock{mu_};
      buf = buf_;
      buf = buf.substr(offset_, buf_.size() - offset_);

      if (!force && buf.size() < kBlockSize) {
        return Error::kOk;
      }

      if (buf.empty()) {
        return Error::kOk;
      }

      offset_ = buf_.size();
    }

    ssize_t w = file_.Write(buf.data(), buf.size());
    if (w != buf.size()) {
      return Error{1};
    }
    return Error::kOk;
  }

  size_t Write(std::string_view buffer) {
    std::unique_lock lock{mu_};
    if (buf_.size() + buffer.size() > capacity_) {
      return -1;
    }

    size_t offset = buf_.size();
    buf_ += buffer;
    return offset;
  }

  Error Sync() {
    if (auto err = Flush(true); err != Error::kOk) {
      return err;
    }
    return file_.Sync();
  }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_WRITABLE_FILE_H
