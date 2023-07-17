#include "pedrolib/file/file.h"
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>
#include "pedrolib/logger/logger.h"

namespace pedrolib {

Logger File::logger = Logger("pedrolib::File");

struct DefaultDeleter {
  void operator()(struct iovec* ptr) const noexcept { std::free(ptr); }
};

ssize_t File::Read(void* buf, size_t size) noexcept {
  return ::read(fd_, buf, size);
}

ssize_t File::Write(const void* buf, size_t size) noexcept {
  return ::write(fd_, buf, size);
}

File& File::operator=(File&& other) noexcept {
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

std::string File::String() const {
  return fmt::format("File[fd={}]", fd_);
}

ssize_t File::Readv(const std::string_view* buf, size_t n) noexcept {
  struct iovec* io;
  std::unique_ptr<struct iovec, DefaultDeleter> cleaner;
  if (n * sizeof(struct iovec) <= 65536) {
    io = static_cast<iovec*>(alloca(sizeof(struct iovec) * n));
  } else {
    io = static_cast<iovec*>(malloc(sizeof(struct iovec) * n));
    cleaner.reset(io);
  }

  for (size_t i = 0; i < n; ++i) {
    io[i].iov_base = const_cast<char*>(buf[i].data());
    io[i].iov_len = buf[i].size();
  }

  return ::readv(fd_, io, static_cast<int>(n));
}

ssize_t File::Writev(std::string_view* buf, size_t n) noexcept {
  struct iovec* io;
  std::unique_ptr<struct iovec, DefaultDeleter> cleaner;
  if (n * sizeof(struct iovec) <= 65536) {
    io = static_cast<iovec*>(alloca(sizeof(struct iovec) * n));
  } else {
    io = static_cast<iovec*>(malloc(sizeof(struct iovec) * n));
    cleaner.reset(io);
  }

  for (size_t i = 0; i < n; ++i) {
    io[i].iov_base = const_cast<char*>(buf[i].data());
    io[i].iov_len = buf[i].size();
  }

  return ::writev(fd_, io, static_cast<int>(n));
}

ssize_t File::Pread(uint64_t offset, void* buf, size_t n) {
  return ::pread64(fd_, buf, n, static_cast<__off64_t>(offset));
}

int64_t File::Seek(uint64_t offset, File::Whence whence) {
  int hint = 0;
  if (whence == Whence::kSeekSet) {
    hint = SEEK_SET;
  } else if (whence == Whence::kSeekCur) {
    hint = SEEK_CUR;
  } else if (whence == Whence::kSeekEnd) {
    hint = SEEK_END;
  }
  auto off = ::lseek64(fd_, static_cast<__off64_t>(offset), hint);
  return static_cast<int64_t>(off);
}
ssize_t File::Pwrite(uint64_t offset, const void* buf, size_t n) {
  return ::pwrite64(fd_, buf, n, static_cast<__off64_t>(offset));
}

Error File::Sync() const noexcept {
  return Error{syncfs(fd_)};
}

File File::Open(const char* name, File::OpenOption option) {
  auto open_flag = [=] {
    int flag = 0;
    if (option.create) {
      flag |= O_CREAT;
    }
    if (option.direct) {
      flag |= O_DIRECT;
    }
    switch (option.mode) {
      case OpenMode::kRead:
        return flag | O_RDONLY;
      case OpenMode::kWrite:
        return flag | O_WRONLY;
      case OpenMode::kReadWrite:
        return flag | O_RDWR;
      default:
        std::terminate();
    }
  };

  int fd;
  if (option.create) {
    fd = ::open(name, open_flag(), option.create.value());
  } else {
    fd = ::open(name, open_flag());
  }

  if (fd <= 0) {
    return File{kInvalid};
  }
  return File{fd};
}

int64_t File::Fill(File& file, char ch, uint64_t n) {
  if (!file.Valid()) {
    return -1;
  }
  std::vector<char> buf(128 << 10, ch);
  uint64_t total = 0;
  uint64_t buf_size = buf.size();
  while (total < n) {
    ssize_t w = file.Pwrite(total, buf.data(), std::min(n - total, buf_size));
    if (w <= 0) {
      return -1;
    }
    total += w;
  }
  return static_cast<int64_t>(total);
}

int64_t File::Size(File& file) {
  int64_t cur = file.Seek(0, Whence::kSeekCur);
  if (cur < 0) {
    return cur;
  }
  int64_t n = file.Seek(0, Whence::kSeekEnd);
  if (n < 0) {
    return n;
  }
  if (file.Seek(cur, Whence::kSeekSet) < 0) {
    return -1;
  }
  return n;
}

Error File::Remove(const char* name) {
  if (::remove(name)) {
    return Error{errno};
  }
  return Error::Success();
}

ssize_t File::Preadv(uint64_t offset, std::string_view* buf, size_t n) {
  struct iovec* io;
  std::unique_ptr<struct iovec, DefaultDeleter> cleaner;
  if (n * sizeof(struct iovec) <= 65536) {
    io = static_cast<iovec*>(alloca(sizeof(struct iovec) * n));
  } else {
    io = static_cast<iovec*>(malloc(sizeof(struct iovec) * n));
    cleaner.reset(io);
  }

  for (size_t i = 0; i < n; ++i) {
    io[i].iov_base = const_cast<char*>(buf[i].data());
    io[i].iov_len = buf[i].size();
  }

  return ::preadv64(fd_, io, static_cast<int>(n),
                    static_cast<__off64_t>(offset));
}

ssize_t File::Pwritev(uint64_t offset, std::string_view* buf, size_t n) {
  struct iovec* io;
  std::unique_ptr<struct iovec, DefaultDeleter> cleaner;
  if (n * sizeof(struct iovec) <= 65536) {
    io = static_cast<iovec*>(alloca(sizeof(struct iovec) * n));
  } else {
    io = static_cast<iovec*>(malloc(sizeof(struct iovec) * n));
    cleaner.reset(io);
  }

  for (size_t i = 0; i < n; ++i) {
    io[i].iov_base = const_cast<char*>(buf[i].data());
    io[i].iov_len = buf[i].size();
  }

  return ::pwritev64(fd_, io, static_cast<int>(n),
                     static_cast<__off64_t>(offset));
}

}  // namespace pedrolib