#ifndef PEDRONET_CORE_FILE_H
#define PEDRONET_CORE_FILE_H

#include "pedronet/core/error.h"

#include <pedrolib/format/formatter.h>
#include <pedrolib/noncopyable.h>
#include <string_view>

namespace pedronet::core {

class File : pedrolib::noncopyable {
public:
  constexpr inline static int kInvalid = -1;

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

  virtual Error GetError() const noexcept { return Error(errno); }

  virtual std::string String() const;
};

} // namespace pedronet::core

PEDROLIB_CLASS_FORMATTER(pedronet::core::File);
#endif // PEDRONET_CORE_FILE_H