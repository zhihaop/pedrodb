#ifndef PEDROLIB_FILE_FILE_H
#define PEDROLIB_FILE_FILE_H

#include "pedrolib/file/error.h"
#include "pedrolib/format/formatter.h"
#include "pedrolib/logger/logger.h"
#include "pedrolib/noncopyable.h"

#include <optional>
#include <string_view>

namespace pedrolib {

class File : pedrolib::noncopyable {
public:
  constexpr inline static int kInvalid = -1;

  enum class Whence { kSeekSet, kSeekCur, kSeekEnd };

  enum class OpenMode { kRead, kWrite, kReadWrite };

  struct OpenOption {
    OpenMode mode{OpenMode::kRead};
    std::optional<int32_t> create{};
  };

protected:
  int fd_{kInvalid};

  static Logger logger;

public:
  File() = default;

  static File Open(const char *name, OpenOption option);

  static Error Remove(const char *name);

  static int64_t Fill(File &file, char ch, uint64_t n);

  static int64_t Size(File &file);

  explicit File(int fd) : fd_(fd) {}

  File(File &&other) noexcept : fd_(other.fd_) { other.fd_ = kInvalid; }

  File &operator=(File &&other) noexcept;

  virtual ssize_t Read(void *buf, size_t n) noexcept;

  virtual ssize_t Write(const void *buf, size_t n) noexcept;

  virtual ssize_t Readv(const std::string_view *buf, size_t n) noexcept;

  virtual ssize_t Writev(std::string_view *buf, size_t n) noexcept;

  virtual ssize_t Pread(uint64_t offset, void *buf, size_t n);
  
  virtual ssize_t Preadv(uint64_t offset, std::string_view *buf, size_t n);

  virtual int64_t Seek(uint64_t offset, Whence whence);

  virtual ssize_t Pwrite(uint64_t offset, const void *buf, size_t n);
  
  virtual ssize_t Pwritev(uint64_t offset, std::string_view *buf, size_t n);

  bool Valid() const noexcept { return fd_ != kInvalid; }

  int Descriptor() const noexcept { return fd_; }

  void Close();

  virtual ~File() { Close(); }

  virtual Error GetError() const noexcept { return Error(errno); }

  virtual std::string String() const;

  Error Sync() const noexcept;
};

Error GetFileSize(const char *filename, uint64_t *n);

} // namespace pedrolib

PEDROLIB_CLASS_FORMATTER(pedrolib::File);
#endif // PEDROLIB_FILE_FILE_H