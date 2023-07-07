#ifndef PEDRODB_FILE_READABLE_FILE_H
#define PEDRODB_FILE_READABLE_FILE_H
#include <pedrolib/noncopyable.h>
namespace pedrodb {

struct ReadableFile : pedrolib::noncopyable {
  ReadableFile() = default;
  virtual ~ReadableFile() = default;
  [[nodiscard]] virtual uint64_t Size() const noexcept = 0;
  [[nodiscard]] virtual Error GetError() const noexcept = 0;
  virtual ssize_t Read(uint64_t offset, char *buf, size_t n) = 0;
};
} // namespace pedrodb

#endif // PEDRODB_FILE_READABLE_FILE_H
