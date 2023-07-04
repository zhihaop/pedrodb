#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

#include <pedrolib/collection/lru_unordered_map.h>

namespace pedrodb {

using ReadableFileGuard =
    std::unique_ptr<ReadableFile, std::function<void(ReadableFile *)>>;

class FileManager {
  mutable std::mutex mu_;
  
  MetadataManager *metadata_;

  pedrolib::lru::unordered_map<uint32_t, ReadableFile> open_files_;
  std::unordered_map<uint32_t, ReadableFile> in_use_;

  // always in use.
  std::unique_ptr<WritableFile> active_;
  uint32_t active_id_{};

  Status OpenActiveFile(WritableFile **file, uint32_t id);

public:
  FileManager(MetadataManager *metadata, uint16_t max_open_files)
      : open_files_(max_open_files), metadata_(metadata) {}

  Status Init();

  std::unique_lock<std::mutex> AcquireLock() const noexcept {
    return std::unique_lock(mu_);
  }

  Status GetActiveFile(WritableFile **file, uint32_t *id) noexcept {
    *file = active_.get();
    *id = active_id_;
    return Status::kOk;
  }

  Status CreateActiveFile(WritableFile **file, uint32_t *id);

  uint32_t GetActiveFileID() const noexcept { return active_id_; }

  Status ReleaseDataFile(uint32_t id);

  Status AcquireDataFile(uint32_t id, ReadableFileGuard *file);

  void Close(uint32_t id) { open_files_.erase(id); }

  Error RemoveDataFile(uint32_t id) {
    Close(id);
    metadata_->DeleteFile(id);
    return File::Remove(metadata_->GetDataFilePath(id).c_str());
  }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_MANAGER_H
