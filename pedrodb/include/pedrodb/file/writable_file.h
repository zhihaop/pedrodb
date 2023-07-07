#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
class WritableFile : noncopyable {
  File file_{};
  uint64_t write_offset_{};

public:
  ~WritableFile() = default;

  static Status OpenOrCreate(const std::string &filename,
                             std::unique_ptr<WritableFile> *file) {
    auto f = std::make_unique<WritableFile>();
    File::OpenOption option{.mode = File ::OpenMode::kReadWrite,
                            .create = 0777};
    f->file_ = File::Open(filename.c_str(), option);
    if (!f->file_.Valid()) {
      PEDRODB_ERROR("failed to create active file {}: {}", filename,
                    f->file_.GetError());
      return Status::kIOError;
    }
    *file = std::move(f);
    return Status::kOk;
  }

  Error ReadAll(std::string *value) {
    int64_t size = File::Size(file_);
    if (size < 0) {
      return file_.GetError();
    }

    value->resize(size);

    ssize_t r = file_.Read(value->data(), size);
    if (r != size) {
      return file_.GetError();
    }

    write_offset_ += r;
    return Error::Success();
  }

  [[nodiscard]] uint64_t GetOffset() const noexcept { return write_offset_; }

  Error Write(const char *buffer, size_t n) {
    ssize_t w = file_.Write(buffer, n);
    if (w != n) {
      return file_.GetError();
    }
    write_offset_ += w;
    return Error ::Success();
  }

  Error Sync() { return file_.Sync(); }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_WRITABLE_FILE_H
