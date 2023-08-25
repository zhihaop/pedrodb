#ifndef PEDRODB_FILE_POSIX_READONLY_FILE_H
#define PEDRODB_FILE_POSIX_READONLY_FILE_H

#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/status.h"

namespace pedrodb {
class PosixReadonlyFile final : public ReadableFile, noncopyable, nonmovable {
 public:
  using Ptr = std::shared_ptr<PosixReadonlyFile>;

 private:
  File file_;
  size_t length_{};

 public:
  PosixReadonlyFile() = default;
  ~PosixReadonlyFile() override = default;

  ssize_t Read(uint64_t offset, char* data, size_t length) override {
    return file_.Pread(offset, data, length);
  }

  [[nodiscard]] uint64_t Size() const noexcept override { return length_; }

  [[nodiscard]] Error GetError() const noexcept override {
    return file_.GetError();
  }

  Status Open(const std::string& path) override {
    File::OpenOption option;
    option.mode = File::OpenMode::kRead;

    file_ = File::Open(path.c_str(), option);
    if (!file_.Valid()) {
      return Status::kIOError;
    }

    length_ = file_.GetSize();
    if (length_ == -1) {
      PEDRODB_ERROR("failed to get size of ptr {}: {}", path, GetError());
      return Status::kIOError;
    }
    return Status::kOk;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_POSIX_READONLY_FILE_H
