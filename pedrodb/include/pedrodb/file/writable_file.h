#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
class WritableFile final : public ReadableFile {
  File file_{};
  ArrayBuffer buf_;
  size_t offset_{};
  const size_t capacity_{};
  const file_t id_{};

  WritableFile(file_t id, size_t capacity)
      : buf_(capacity), capacity_(capacity), id_(id) {}

 public:
  ~WritableFile() override { Flush(true); }

  static Status Open(const std::string& filename, file_t id, size_t capacity,
                     WritableFile** f) {
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

    auto ptr = *f = new WritableFile(id, capacity);
    ptr->file_ = std::move(file);
    ptr->offset_ = buf.size();
    ptr->buf_.Append(buf.data(), buf.size());
    return Status::kOk;
  }

  [[nodiscard]] uint64_t Size() const noexcept override { return buf_.ReadableBytes(); }

  [[nodiscard]] Error GetError() const noexcept override { return file_.GetError(); }

  [[nodiscard]] file_t GetFile() const noexcept { return id_; }

  ssize_t Read(uint64_t offset, char* buf, size_t n) override {
    memcpy(buf, buf_.ReadIndex() + offset, n);
    return static_cast<ssize_t>(n);
  }

  Error Flush(bool force) {
    const char* offset = buf_.ReadIndex() + offset_;
    size_t flush = buf_.ReadableBytes() - offset_;

    if (!force && flush < kBlockSize) {
      return Error::kOk;
    }

    if (flush == 0) {
      return Error::kOk;
    }

    offset_ = buf_.ReadableBytes();

    ssize_t w = file_.Write(offset, flush);
    if (w != flush) {
      return Error{1};
    }
    return Error::kOk;
  }

  template <typename Key, typename Value>
  size_t Write(const record::Entry<Key, Value>& entry) {
    if (buf_.ReadableBytes() + entry.SizeOf() > capacity_) {
      return -1;
    }

    size_t offset = buf_.ReadableBytes();
    entry.Pack(&buf_);

    PEDRODB_IGNORE_ERROR(Flush(false));
    return offset;
  }

  Error Sync() {
    if (auto err = Flush(true); err != Error::kOk) {
      return err;
    }
    return file_.Sync();
  }
};

}  // namespace pedrodb
#endif  // PEDRODB_FILE_WRITABLE_FILE_H
