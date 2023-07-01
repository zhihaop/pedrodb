#ifndef PEDRONET_CORE_FILE_H
#define PEDRONET_CORE_FILE_H

#include "pedronet/core/debug.h"
#include "pedronet/core/noncopyable.h"

#include <string_view>

namespace pedronet::core {

class File : noncopyable {
public:
  constexpr inline static int kInvalid = -1;

  class Error {
    int code_{};

  public:
    explicit Error(int code) : code_(code) {}

    static File::Error Success() { return {}; }

    Error() = default;
    ~Error() = default;
    Error(const Error &) = default;
    Error &operator=(const Error &) = default;

    bool Empty() const noexcept { return code_ == 0; }
    bool operator==(const Error &err) const noexcept {
      return code_ == err.code_;
    }
    int GetCode() const noexcept { return code_; }
    const char *GetReason() const noexcept;
    void Clear() { code_ = 0; }

    std::string String() const {
      return fmt::format("Error[code:{}, reason:{}]", code_, GetReason());
    }
  };

protected:
  int fd_{kInvalid};

public:
  File() = default;

  explicit File(int fd) : fd_(fd) {}

  File(File &&other) noexcept : fd_(other.fd_) { other.fd_ = kInvalid; }

  File &operator=(File &&other) noexcept;

  virtual ssize_t Read(void *buf, size_t size) noexcept;

  virtual ssize_t Write(const void *buf, size_t size) noexcept;

  virtual ssize_t Readv(const std::string_view *buf, size_t n) noexcept;

  virtual ssize_t Writev(std::string_view *buf, size_t n) noexcept;

  bool Valid() const noexcept { return fd_ != kInvalid; }

  int Descriptor() const noexcept { return fd_; }

  void Close();

  virtual ~File() { Close(); }

  virtual File::Error GetError() const noexcept { return File::Error(errno); }

  virtual std::string String() const;
};

} // namespace pedronet::core

PEDRONET_CLASS_FORMATTER(pedronet::core::File::Error);
PEDRONET_CLASS_FORMATTER(pedronet::core::File);
#endif // PEDRONET_CORE_FILE_H