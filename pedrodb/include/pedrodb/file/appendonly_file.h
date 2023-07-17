#ifndef PEDRODB_FILE_APPENDONLY_FILE_H
#define PEDRODB_FILE_APPENDONLY_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"

#include <fcntl.h>

namespace pedrodb {
struct FileCleaner {
  void operator()(FILE* file) const noexcept {
    if (file != nullptr) {
      std::fclose(file);
    }
  }
};

class AppendOnlyFile : public WritableFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<AppendOnlyFile>;

 private:
  using FileGuard = std::unique_ptr<FILE, FileCleaner>;

  FileGuard file_;
  ArrayBuffer buffer_;

 public:
  AppendOnlyFile() = default;
  ~AppendOnlyFile() override = default;

  template <class WritableFilePtr>
  static Status Open(const std::string& path, WritableFilePtr* file) {
    FileGuard f = FileGuard(fopen(path.c_str(), "a"), FileCleaner{});
    if (f == nullptr) {
      return Status::kIOError;
    }
    auto ptr = new AppendOnlyFile();
    ptr->file_ = std::move(f);
    file->reset(ptr);
    return Status::kOk;
  }

  Error Flush(bool force) override {
    if (file_ != nullptr) {
      if (fflush(file_.get()) != 0) {
        return Error{errno};
      }
    }
    return Error::kOk;
  }

  template <typename Packable>
  void Write(Packable&& entry) {
    buffer_.Reset();
    entry.Pack(&buffer_);
    fwrite(buffer_.ReadIndex(), buffer_.ReadableBytes(), 1, file_.get());
    PEDRODB_IGNORE_ERROR(Flush(false));
  }

  Error Sync() override {
    if (file_ != nullptr) {
      if (fsync(fileno(file_.get())) != 0) {
        return Error{errno};
      }
    }
    return Error::kOk;
  }
};
}  // namespace pedrodb

#endif  //PEDRODB_FILE_APPENDONLY_FILE_H
