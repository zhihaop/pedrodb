#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/cache/lru_cache.h"
#include "pedrodb/cache/segment_cache.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/mapping_readonly_file.h"
#include "pedrodb/file/mapping_readwrite_file.h"
#include "pedrodb/file/posix_readonly_file.h"
#include "pedrodb/file/posix_readwrite_file.h"
#include "pedrodb/format/index_format.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

namespace pedrodb {

class FileManager : public std::enable_shared_from_this<FileManager> {
  mutable SpinLock mu_;

  MetadataManager::Ptr metadata_manager_;
  LRUCache<file_id_t, ReadableFile::Ptr> open_files_;

  // always in use.
  std::shared_ptr<ArrayBuffer> active_index_log_;
  ReadWriteFile::Ptr active_data_file_;
  file_id_t active_file_id_{};

  std::shared_ptr<Executor> executor_{};

  Status CreateFile(file_id_t id);

  Status Recovery(file_id_t active);

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

 public:
  using Ptr = std::shared_ptr<FileManager>;

  FileManager(MetadataManager::Ptr metadata_manager,
              std::shared_ptr<Executor> executor, uint8_t max_open_files)
      : open_files_(max_open_files),
        executor_(std::move(executor)),
        metadata_manager_(std::move(metadata_manager)) {}

  Status Init();

  Status Sync();

  Status Flush(bool force);

  template <typename Key, typename Value>
  Status Append(const record::Entry<Key, Value>& entry, record::Location* loc) {
    for (;;) {
      auto lock = AcquireLock();
      auto file_id = active_file_id_;
      auto data_file = active_data_file_;
      auto index_log = active_index_log_;
      lock.unlock();
      
      auto flock = data_file->GetLock();
      WritableBuffer buffer = data_file->Allocate(entry.SizeOf());
      if (buffer.GetOffset() != -1) {
        entry.Pack(&buffer);
        data_file->Flush(false);

        loc->offset = buffer.GetOffset();
        loc->id = file_id;

        index::Entry<Key> index_entry;
        index_entry.type = entry.type;
        index_entry.key = entry.key;
        index_entry.offset = buffer.GetOffset();
        index_entry.len = entry.SizeOf();

        index_entry.Pack(index_log.get());
        return Status::kOk;
      }
      flock.unlock();
      
      lock.lock();
      if (file_id != active_file_id_) {
        continue;
      }
      
      auto status = CreateFile(file_id + 1);
      if (status != Status::kOk) {
        return status;
      }
    }
  }

  void ReleaseDataFile(file_id_t id);

  Status AcquireDataFile(file_id_t id, ReadableFile::Ptr* file);

  Status AcquireIndexFile(file_id_t id, ReadableFile::Ptr* file);

  Status RemoveFile(file_id_t id);

  void SyncFile(file_id_t id, const ReadWriteFile::Ptr& file);

  void CreateIndexFile(file_id_t id, const std::shared_ptr<ArrayBuffer>& log);
};
}  // namespace pedrodb
#endif  // PEDRODB_FILE_MANAGER_H
