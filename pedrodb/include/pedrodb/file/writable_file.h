#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
// TODO add file lock
class WritableFile {
  uint32_t file_id_{};
  uint32_t offset_{};
  File file_{};

public:
  ~WritableFile() {
    PEDRODB_INFO("close active: {}", file_id_);
    if (file_.Valid()) {
      file_.Sync();
    }
  }
  static std::shared_ptr<WritableFile> Create(uint32_t file_id,
                                              const std::string &filename) {
    auto file = std::make_shared<WritableFile>();
    file->file_id_ = file_id;
    int fd = open(filename.c_str(), O_CREAT | O_RDWR, 0777);
    if (fd < 0) {
      PEDRODB_ERROR("failed to create active file");
      return nullptr;
    }
    file->file_ = pedrolib::File(fd);
    file->offset_ = file->file_.Seek(0, File::Whence::kSeekEnd);
    return file;
  }

  uint32_t GetID() const noexcept { return file_id_; }
  uint32_t GetOffSet() const noexcept { return offset_; }

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
