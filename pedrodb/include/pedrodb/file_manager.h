#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/defines.h"
#include "pedrodb/file/random_access_file.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata.h"
#include <pedrolib/collection/lru_unordered_map.h>

#include <list>
namespace pedrodb {
class FileManager {
  MetadataManager *metadata_;

  std::mutex mu_;

  pedrolib::lru::unordered_map<uint32_t, std::shared_ptr<RandomAccessFile>>
      open_files_;
  std::shared_ptr<WritableFile> active_;

  std::shared_ptr<RandomAccessFile> open(uint32_t file_id) {
    std::string filename = metadata_->GetFile(file_id);
    auto file = RandomAccessFile::Open(filename);
    if (file == nullptr) {
      PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, file_id);
      return nullptr;
    }
    return std::move(file);
  }

  Error createActiveFile(std::shared_ptr<WritableFile> *active, uint32_t id) {
    PEDRODB_INFO("create active file {}", id);
    *active = WritableFile::Create(id, metadata_->GetFile(id));
    if (*active == nullptr) {
      return Error{errno};
    }
    return Error::Success();
  }

public:
  FileManager(MetadataManager *metadata, uint16_t max_open_files)
      : open_files_(max_open_files), metadata_(metadata) {}

  Status Init() {
    size_t active_id;
    if (metadata_->GetActiveID() == 0) {
      active_id = metadata_->AddActiveID();
    } else {
      active_id = metadata_->GetActiveID();
    }

    auto err = createActiveFile(&active_, active_id);
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to load active file: {}", err);
      return Status::kIOError;
    }
    PEDRODB_INFO("load active file: {}", metadata_->GetActiveID());
    return Status::kOk;
  }

  std::shared_ptr<WritableFile> GetActiveFile() noexcept {
    std::unique_lock<std::mutex> lock(mu_);
    return active_;
  }

  Error CreateActiveFile() {
    std::unique_lock<std::mutex> lock(mu_);
    return createActiveFile(&active_, metadata_->AddActiveID());
  }

  std::shared_ptr<RandomAccessFile> GetFile(uint32_t file_id) {
    std::unique_lock<std::mutex> lock(mu_);
    if (open_files_.contains(file_id)) {
      return open_files_[file_id];
    }

    PEDRODB_INFO("open file {}", file_id);

    auto file = open(file_id);
    if (file == nullptr) {
      return nullptr;
    }

    open_files_.update(file_id, file);
    return file;
  }

  void Release() {
    std::unique_lock<std::mutex> lock(mu_);
    open_files_.clear();
  }

  void Close(uint32_t file_id) {
    std::unique_lock<std::mutex> lock(mu_);
    open_files_.erase(file_id);
  }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_MANAGER_H
