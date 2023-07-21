#ifndef PEDRODB_FILE_READWRITE_FILE_H
#define PEDRODB_FILE_READWRITE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"

#include <sys/mman.h>
namespace pedrodb {

class ReadWriteFile final : public ReadableFile,
                            public WritableFile,
                            noncopyable,
                            nonmovable {
 public:
  using Ptr = std::shared_ptr<ReadWriteFile>;

 private:
  File file_;
  char* data_{};
  size_t write_index_{};
  const size_t capacity_{};

  explicit ReadWriteFile(size_t capacity) : capacity_(capacity) {}

 public:
  ~ReadWriteFile() override {
    if (data_ != nullptr) {
      munmap(data_, capacity_);
      madvise(data_, capacity_, MADV_DONTNEED);
      data_ = nullptr;
    }
  }

  char* WriteIndex() noexcept { return data_ + write_index_; }

  [[nodiscard]] size_t WritableBytes() const noexcept {
    return capacity_ - write_index_;
  }

  void SetWriteIndex(size_t index) noexcept { write_index_ = index; }

  [[nodiscard]] ReadonlyBuffer GetReadonlyBuffer() const noexcept {
    return {data_, capacity_};
  }

  void Append(const char* data, size_t n) {
    memcpy(WriteIndex(), data, n);
    Append(n);
  }

  void Append(size_t n) noexcept { write_index_ += n; }

  template <class WritableFilePtr>
  static Status Open(const std::string& path, size_t capacity,
                     WritableFilePtr* f) {
    File::OpenOption option;
    option.create = 0777;
    option.mode = File::OpenMode::kReadWrite;

    auto file = File::Open(path.c_str(), option);
    if (!file.Valid()) {
      PEDRODB_ERROR("failed to open active file {}: {}", path, Error{errno});
      return Status::kIOError;
    }

    int64_t size = File::Size(file);
    if (size < 0) {
      PEDRODB_ERROR("failed to get size of file {}", Error{errno});
      return Status::kIOError;
    }

    if (size == 0) {
      if (file.Seek(capacity - 1, File::Whence::kSeekSet) < 0) {
        PEDRODB_ERROR("failed to create file {}", file.GetError());
        return Status::kIOError;
      }

      char buf{};
      if (file.Write(&buf, 1) < 0) {
        PEDRODB_ERROR("failed to resize file {}", file.GetError());
        return Status::kIOError;
      }
    } else if (size != capacity) {
      PEDRODB_ERROR("Format error: file size is not correct");
      return Status::kCorruption;
    }

    char* data = (char*)::mmap(nullptr, capacity, PROT_READ | PROT_WRITE,
                               MAP_SHARED, file.Descriptor(), 0);
    if (data == reinterpret_cast<char*>(-1)) {
      PEDRODB_ERROR("failed to mmap file {}", file.GetError());
      return Status::kIOError;
    }

    auto ptr = new ReadWriteFile(capacity);
    f->reset(ptr);
    ptr->file_ = std::move(file);
    ptr->data_ = data;
    return Status::kOk;
  }

  [[nodiscard]] uint64_t Size() const noexcept override { return capacity_; }

  [[nodiscard]] Error GetError() const noexcept override {
    return file_.GetError();
  }

  ssize_t Read(uint64_t offset, char* buf, size_t n) override {
    memcpy(buf, data_ + offset, n);
    return static_cast<ssize_t>(n);
  }

  Error Flush(bool force) override { return Error::kOk; }

  template <typename Packable>
  size_t Write(Packable&& entry) {
    if (entry.SizeOf() > WritableBytes()) {
      return -1;
    }

    size_t offset = write_index_;
    entry.Pack(this);

    PEDRODB_IGNORE_ERROR(Flush(false));
    return offset;
  }

  Error Sync() override {
    if (msync(data_, capacity_, MS_SYNC) < 0) {
      return file_.GetError();
    }
    return Error::kOk;
  }
};

}  // namespace pedrodb
#endif  // PEDRODB_FILE_READWRITE_FILE_H
