#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
class WritableFile : noncopyable {
  ArrayBuffer buffer_{};
  uint64_t offset_{};
  File file_{};

public:
  ~WritableFile() {
    if (file_.Valid()) {
      Flush();
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

  Error Fill(size_t bytes) {
    if (file_.Valid()) {
      if (File::Fill(file_, 0, bytes) < 0) {
        return file_.GetError();
      }
    }
    return Error::Success();
  }

  uint64_t GetOffSet() const noexcept {
    return offset_; 
  }

  Error Write(pedrolib::Buffer *buffer) {
    if (buffer_.ReadableBytes() + buffer->ReadableBytes() > kBlockSize) {
      std::string_view sv[2]{{buffer_.ReadIndex(), buffer_.ReadableBytes()},
                             {buffer->ReadIndex(), buffer->ReadableBytes()}};
      ssize_t w = file_.Writev(sv, 2);
      if (w < 0) {
        return file_.GetError();
      }
      offset_ += buffer->ReadableBytes();
      buffer_.Retrieve(buffer_.ReadableBytes());
      buffer->Retrieve(buffer->ReadableBytes());
      return Error::Success(); return Error::Success();
    }

    offset_ += buffer->ReadableBytes();
    buffer_.Append(buffer);
    return Error::Success();
  }

  Error Sync() { return file_.Sync(); }

  Error Flush() {
    if (buffer_.ReadableBytes()) {
      if (buffer_.Retrieve(&file_) < 0) {
        return file_.GetError();
      }
    }
    return Error::Success();
  }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_WRITABLE_FILE_H
