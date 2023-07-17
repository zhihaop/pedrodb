#ifndef PEDRODB_FILE_READONLY_FILE_H
#define PEDRODB_FILE_READONLY_FILE_H

#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/status.h"

namespace pedrodb {
class ReadonlyFile : public ReadableFile, noncopyable {
 public:
  using Ptr = std::shared_ptr<ReadonlyFile>;

 private:
  uint64_t size_{};
  mutable File file_{};

 public:
  ReadonlyFile() = default;
  ~ReadonlyFile() override = default;
  ReadonlyFile(ReadonlyFile&& other) noexcept
      : size_(other.size_), file_(std::move(other.file_)) {}

  ReadonlyFile& operator=(ReadonlyFile&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    size_ = other.size_;
    file_ = std::move(other.file_);
    other.size_ = 0;
    return *this;
  }

  ssize_t Read(uint64_t offset, char* data, size_t length) override {
    return file_.Pread(offset, data, length);
  }

  uint64_t Size() const noexcept override { return File::Size(file_); }

  Error GetError() const noexcept override { return file_.GetError(); }

  static Status Open(const std::string& filename,
                     std::shared_ptr<ReadableFile>* file) {
    File::OpenOption option{.mode = File ::OpenMode::kRead};
    auto f = File::Open(filename.c_str(), option);
    if (!f.Valid()) {
      PEDRODB_ERROR("failed to open active file {}: {}", filename,
                    f.GetError());
      return Status::kIOError;
    }

    auto ptr = new ReadonlyFile();
    ptr->file_ = std::move(f);
    file->reset(ptr);
    return Status::kOk;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_FILE_READONLY_FILE_H
