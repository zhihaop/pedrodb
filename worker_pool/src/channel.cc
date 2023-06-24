#include "channel.h"

#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
#include <vector>

namespace pedro {

void ZeroCopyOutputChannel::close() {
  if (fd_ != 0) {
    ::close(fd_);
    fd_ = 0;
  }
}

void ZeroCopyInputChannel::close() {
  if (fd_ != 0) {
    ::close(fd_);
    fd_ = 0;
  }
}

void BufferedOutputChannel::close() {
  if (fd_ != 0) {
    flush();
    ::close(fd_);
    fd_ = 0;
  }
}

void BufferedOutputChannel::flush() {
  while (size_ > 0) {
    ssize_t w;
    if (head_ >= tail_) {
      w = write(fd_, &buf_[head_], buf_.size() - head_);
    } else {
      w = write(fd_, &buf_[head_], tail_ - head_);
    }
    if (w <= 0) {
      break;
    }

    size_ -= w;
    head_ = (head_ + w) % buf_.size();
  }
}

void BufferedOutputChannel::write_buffer(char ch) {
  buf_[tail_++] = ch;
  ++size_;
  tail_ = tail_ >= buf_.size() ? 0 : tail_;
}

ssize_t BufferedOutputChannel::Write(void *data, size_t s) {
  char* buf = reinterpret_cast<char*>(data);
  size_t c = 0;
  while (c < s) {
    if (buf_.size() > size_) {
      write_buffer(buf[c++]);
    } else {
      if (s - c >= buf_.size()) {
        break;
      }
      flush();
    }
  }
  if (c != s) {
    flush();
    ssize_t w = ::write(fd_, buf + c, s - c);
    if (w <= 0) {
      return w;
    }
    c += w;
  }
  return s;
}

std::unique_ptr<Channel> Channel::CreateChannel() {
  int fds[2];
  if (pipe(fds) == -1) {
    return {};
  }
  return std::make_unique<Channel>(fds[1], fds[0]);
}

bool Channel::Write(const Message &msg, bool sync) {
  if (!out_.Valid()) {
    return false;
  }

  if (in_.Valid()) {
    in_.Close();
  }

  msg.SerializeToString(&buf_);
  auto size = static_cast<uint32_t>(buf_.size());

  out_.Write(&size, sizeof(size));
  out_.Write(buf_.data(), buf_.size());
  if (sync) {
    out_.Flush();
  }
  return true;
}

bool Channel::Read(Message &msg) {
  if (!in_.Valid()) {
    return false;
  }

  if (out_.Valid()) {
    out_.Close();
  }

  uint32_t size;
  in_.Read(&size, sizeof(size));

  buf_.resize(size);
  in_.Read(buf_.data(), buf_.size());
  return msg.ParseFromString(buf_);
}

bool Channel::Valid() const noexcept { return in_.Valid() || out_.Valid(); }

void Channel::Close() {
  if (in_.Valid()) {
    in_.Close();
  }

  if (out_.Valid()) {
    out_.Close();
  }
}
} // namespace pedro