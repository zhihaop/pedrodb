#ifndef PEDRONET_CORE_FILE_H
#define PEDRONET_CORE_FILE_H

#include "noncopyable.h"
#include <spdlog/spdlog.h>

namespace pedronet {
namespace core {

class File : noncopyable {
public:
  constexpr inline static int kInvalid = -1;

  class Error {
    int code_{};

  public:
    explicit Error(int code) : code_(code) {}
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
  };

protected:
  int fd_{-1};

public:
  File() = default;

  explicit File(int fd) : fd_(fd) {}

  File(File &&other) : fd_(other.fd_) { other.fd_ = kInvalid; }

  File &operator=(File &&other) noexcept;

  ssize_t Read(void *buf, size_t size) noexcept;

  ssize_t Write(const void *buf, size_t size) noexcept;

  bool Valid() const noexcept { return fd_ != kInvalid; }

  int Descriptor() const noexcept { return fd_; }

  void Close();

  virtual ~File() { Close(); }

  virtual File::Error GetError() const noexcept { return File::Error(errno); }
};
} // namespace core
} // namespace pedronet
#endif // PEDRONET_CORE_FILE_H