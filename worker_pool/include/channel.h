#ifndef PRACTICE_CHANNEL_H
#define PRACTICE_CHANNEL_H

#include "proto/rpc.pb.h"
#include <memory>
#include <unistd.h>
#include <vector>

namespace pedro {

using Message = google::protobuf::Message;

class CloseableChannel {
public:
  CloseableChannel() = default;
  CloseableChannel(const CloseableChannel &) = delete;
  CloseableChannel(CloseableChannel &&) noexcept = delete;
  CloseableChannel &operator=(const CloseableChannel &) = delete;
  CloseableChannel &operator=(CloseableChannel &&) = delete;
  virtual ~CloseableChannel() = default;

  virtual bool Valid() const = 0;
  virtual void Close() = 0;
  virtual int Handler() = 0;
};

class InputChannel : public CloseableChannel {
public:
  InputChannel() : CloseableChannel() {}
  ~InputChannel() override = default;

  virtual ssize_t Read(void *data, size_t s) = 0;
};

class OutputChannel : public CloseableChannel {
public:
  OutputChannel() : CloseableChannel() {}
  ~OutputChannel() override = default;

  virtual ssize_t Write(void *data, size_t s) = 0;
  virtual void Flush() = 0;
};

class ZeroCopyOutputChannel : public OutputChannel {
  int fd_{};

  void close();

public:
  explicit ZeroCopyOutputChannel(int fd) : OutputChannel(), fd_(fd) {}
  ~ZeroCopyOutputChannel() { close(); }

  void Close() override { close(); }
  bool Valid() const override { return fd_; }
  int Handler() override { return fd_; }
  ssize_t Write(void *data, size_t s) override { return write(fd_, data, s); }
  void Flush() override {}
};

class BufferedOutputChannel : public OutputChannel {
  int fd_{};

  std::vector<char> buf_;
  size_t head_{};
  size_t tail_{};
  size_t size_{};

  void close();
  void flush();
  void write_buffer(char ch);

public:
  BufferedOutputChannel(int fd, size_t buffer_size)
      : OutputChannel(), fd_(fd), buf_(buffer_size) {}

  explicit BufferedOutputChannel(int fd) : BufferedOutputChannel(fd, 4096) {}

  ~BufferedOutputChannel() { close(); }

  void Close() override { close(); }

  bool Valid() const override { return fd_; }

  int Handler() override { return fd_; }

  ssize_t Write(void *data, size_t s) override;

  void Flush() override { flush(); }
};

class ZeroCopyInputChannel : public InputChannel {
  int fd_{};

  void close();

public:
  explicit ZeroCopyInputChannel(int fd) : InputChannel(), fd_(fd) {}
  ~ZeroCopyInputChannel() { close(); }

  void Close() override { close(); }
  bool Valid() const override { return fd_; }
  int Handler() override { return fd_; }
  ssize_t Read(void *data, size_t s) override {
    char* buf = reinterpret_cast<char*>(data);
    size_t c = 0;
    while (s > 0) {
      ssize_t r = read(fd_, buf, s);
      if (r <= 0) {
        return r;
      }
      s -= r;
      buf += r;
      c += r;
    }
    return c;
  }
};

class Channel {
  BufferedOutputChannel out_;
  ZeroCopyInputChannel in_;

  std::string buf_;

public:
  Channel(int out, int in) : out_(out), in_(in) {}
  Channel(const Channel &ch) = delete;
  Channel &operator=(const Channel &ch) = delete;
  Channel(Channel &&ch) noexcept = delete;
  Channel &operator=(Channel &&ch) noexcept = delete;

  ~Channel() { Close(); }

  static std::unique_ptr<Channel> CreateChannel();

  bool Write(const Message &msg, bool sync);
  bool Read(Message &msg);
  bool Valid() const noexcept;
  void Close();
};
} // namespace pedro
#endif // PRACTICE_CHANNEL_H