#include "pedronet/core/file.h"
#include <cerrno>

namespace pedronet {
namespace core {

const char *File::Error::GetReason() const noexcept {
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
  fd_ = other.fd_;

  return *this;
}

void File::Close() {
  if (fd_ >= 0) {
    spdlog::info("close fd[{}]", fd_);
    ::close(fd_);
    fd_ = kInvalid;
  }
}

} // namespace core
} // namespace pedronet