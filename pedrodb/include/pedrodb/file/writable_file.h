#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
class WritableFile : noncopyable {
  uint64_t offset_{};
  File file_{};

public:
  ~WritableFile() {
    if (file_.Valid()) {
      file_.Sync();
    }
  }

  static Status OpenOrCreate(const std::string &filename, WritableFile **file) {
    auto f = std::make_unique<WritableFile>();
    File::OpenOption option{.mode = File ::OpenMode::kWrite, .create = 0777};
    f->file_ = File::Open(filename.c_str(), option);
    if (!f->file_.Valid()) {
      PEDRODB_ERROR("failed to create active file {}: {}", filename,
                    f->file_.GetError());
      return Status::kIOError;
    }

    int64_t offset = f->file_.Seek(0, File::Whence::kSeekEnd);
    if (offset < 0) {
      PEDRODB_ERROR("failed to seek tail {}: {}", filename,
                    f->file_.GetError());
      return Status::kIOError;
    }

    f->offset_ = offset;
    *file = f.release();
    return Status::kOk;
  }

  void SetOffset(uint64_t offset) { offset_ = offset; }

  Error Reserve(size_t bytes) {
    if (file_.Valid()) {
      int64_t cur = file_.Seek(0, File::Whence::kSeekCur);
      if (cur < 0) {
        return file_.GetError();
      }
      if (File::Fill(file_, 0, bytes) < 0) {
        return file_.GetError();
      }
      if (file_.Seek(cur, File::Whence::kSeekSet) < 0) {
        return file_.GetError();
      }
    }
    return Error::Success();
  }

  uint64_t GetOffSet() const noexcept { return offset_; }

  ssize_t Write(pedrolib::Buffer *buffer) {
    ssize_t w = buffer->Retrieve(&file_);
    if (w > 0) {
      offset_ += w;
    }
    return w;
  }

  Error Flush() { return file_.Sync(); }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_WRITABLE_FILE_H
