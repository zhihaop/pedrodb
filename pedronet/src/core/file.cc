#include "pedronet/core/file.h"
#include "pedronet/logger/logger.h"
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

namespace pedronet::core {

struct DefaultDeleter {
  void operator()(struct iovec *ptr) const noexcept { std::free(ptr); }
};

const char *Error::GetReason() const noexcept {
  thread_local char buf[1024];
  return strerror_r(code_, buf, sizeof(buf));
}

ssize_t File::Read(void *buf, size_t size) noexcept {
  return ::read(fd_, buf, size);
}

ssize_t File::Write(const void *buf, size_t size) noexcept {
  return ::write(fd_, buf, size);
}

File &File::operator=(File &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  Close();
  std::swap(fd_, other.fd_);
  return *this;
}

void File::Close() {
  if (fd_ <= 0) {
    return;
  }
  PEDRONET_INFO("{}::Close()", *this);
  ::close(fd_);
  fd_ = kInvalid;
}

std::string File::String() const { return fmt::format("File[fd={}]", fd_); }

ssize_t File::Readv(const std::string_view *buf, size_t n) noexcept {
  struct iovec *io;
  std::unique_ptr<struct iovec, DefaultDeleter> cleaner;
  if (n * sizeof(struct iovec) <= 65536) {
    io = static_cast<iovec *>(alloca(sizeof(struct iovec) * n));
  } else {
    io = static_cast<iovec *>(malloc(sizeof(struct iovec) * n));
    cleaner.reset(io);
  }

  for (size_t i = 0; i < n; ++i) {
    io[i].iov_base = const_cast<char *>(buf[i].data());
    io[i].iov_len = buf[i].size();
  }

  return ::readv(fd_, io, static_cast<int>(n));
}
ssize_t File::Writev(std::string_view *buf, size_t n) noexcept {
  struct iovec *io;
  std::unique_ptr<struct iovec, DefaultDeleter> cleaner;
  if (n * sizeof(struct iovec) <= 65536) {
    io = static_cast<iovec *>(alloca(sizeof(struct iovec) * n));
  } else {
    io = static_cast<iovec *>(malloc(sizeof(struct iovec) * n));
    cleaner.reset(io);
  }

  for (size_t i = 0; i < n; ++i) {
    io[i].iov_base = const_cast<char *>(buf[i].data());
    io[i].iov_len = buf[i].size();
  }

  return ::writev(fd_, io, static_cast<int>(n));
}

} // namespace pedronet::core