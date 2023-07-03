#ifndef PEDRODB_FILE_RANDOM_ACCESS_FILE_H
#define PEDRODB_FILE_RANDOM_ACCESS_FILE_H

#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"
#include <sys/mman.h>

namespace pedrodb {
// TODO add file lock
class RandomAccessFile : noncopyable {
  File file_;

  char *data_ = nullptr;
  size_t length_ = 0;

public:
  RandomAccessFile() = default;
  explicit RandomAccessFile(File file, size_t length)
      : file_(std::move(file)), length_(length) {
    if (length == 0) {
      return;
    }

    if (file_.Valid()) {
      data_ =
          static_cast<char *>(::mmap(nullptr, length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, file_.Descriptor(), 0));

      if (reinterpret_cast<uintptr_t>(data_) == -1) {
        PEDRODB_FATAL("failed to bind file={}: {}", file_, Error{errno});
      }
      length_ = length;
    }
  }

  RandomAccessFile(RandomAccessFile &&other) noexcept
      : file_(std::move(other.file_)), data_(other.data_),
        length_(other.length_) {
    other.data_ = nullptr;
    other.length_ = 0;
  }

  RandomAccessFile &operator=(RandomAccessFile &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    Close();

    std::swap(other.file_, file_);
    std::swap(other.data_, data_);
    std::swap(other.length_, length_);
    return *this;
  }

  ~RandomAccessFile() { Close(); }

  void Close() {
    PEDRODB_INFO("close file {}", file_);
    if (data_ != nullptr) {
      munmap((void *)data_, length_);
      data_ = nullptr;
      length_ = 0;
    }
    file_.Close();
  }

  Error Flush() {
    if (data_ == nullptr) {
      return Error::Success();
    }
    if (::msync(data_, length_, MS_SYNC) < 0) {
      return Error{errno};
    }
    return Error::Success();
  }

  size_t Size() const noexcept { return length_; }
  const char *Data() const noexcept { return data_; }
  char *Data() noexcept { return data_; }

  const char &operator[](size_t index) const noexcept { return data_[index]; }
  char &operator[](size_t index) noexcept { return data_[index]; }

  BufferSlice Slice(size_t index, size_t length) const noexcept {
    return {data_ + index, length};
  }

  template <class T> const T &At(size_t index) const noexcept {
    return reinterpret_cast<T *>(data_)[index];
  }

  template <class T> T &At(size_t index) noexcept {
    return reinterpret_cast<T *>(data_)[index];
  }

  static std::shared_ptr<RandomAccessFile> Open(const std::string &filename) {
    if (filename.empty()) {
      return nullptr;
    }

    uint64_t filesize;
    auto err = pedrolib::GetFileSize(filename.c_str(), &filesize);
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to get filesize of {}: {}", filename, err);
      return nullptr;
    }

    int fd = ::open(filename.c_str(), O_RDWR);
    if (fd <= 0) {
      PEDRODB_ERROR("failed to open file {}: {}", filename, Error{errno});
      // TODO handle error
      return nullptr;
    }
    return std::make_shared<RandomAccessFile>(pedrolib::File(fd), filesize);
  }
};

} // namespace pedrodb

#endif // PEDRODB_FILE_RANDOM_ACCESS_FILE_H