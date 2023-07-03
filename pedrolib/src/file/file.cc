#include "pedrolib/file/file.h"
#include "pedrolib/logger/logger.h"
#include <sys/uio.h>
#include <unistd.h>

namespace pedrolib {

Logger File::logger = Logger("pedrolib::File");

struct DefaultDeleter {
  void operator()(struct iovec *ptr) const noexcept { std::free(ptr); }
};

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
  logger.Trace("{}::Close()", *this);
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
ssize_t File::Pread(uint64_t offset, void *buf, size_t n) {
  return ::pread64(fd_, buf, n, static_cast<__off64_t>(offset));
}
uint64_t File::Seek(uint64_t offset, File::Whence whence) {
  int hint = 0;
  if (whence == Whence::kSeekSet) {
    hint = SEEK_SET;
  } else if (whence == Whence::kSeekCur) {
    hint = SEEK_CUR;
  } else if (whence == Whence::kSeekEnd) {
    hint = SEEK_END;
  }
  auto off = ::lseek64(fd_, static_cast<__off64_t>(offset), hint);
  return static_cast<uint64_t>(off);
}
ssize_t File::Pwrite(uint64_t offset, const void *buf, size_t n) {
  return ::pwrite64(fd_, buf, n, static_cast<__off64_t>(offset));
}
Error File::Sync() const noexcept { return Error{syncfs(fd_)}; }

Error GetFileSize(const char *filename, uint64_t *n) {
  struct stat buf {};
  if (stat(filename, &buf)) {
    return Error{errno};
  }
  *n = static_cast<uint64_t>(buf.st_size);
  return Error::Success();
}
} // namespace pedrolib