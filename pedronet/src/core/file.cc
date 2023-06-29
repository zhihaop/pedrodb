#include "pedronet/core/file.h"

namespace pedronet::core {

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
  fd_ = other.fd_;
  other.fd_ = kInvalid;

  return *this;
}

void File::Close() {
  if (fd_ <= 0) {
    return;
  }
  spdlog::info("{}::Close()", *this);
  ::close(fd_);
  fd_ = kInvalid;
}
std::string File::String() const { return fmt::format("File[fd={}]", fd_); }

} // namespace pedronet::core