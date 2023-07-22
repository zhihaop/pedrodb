#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/defines.h"
#include "pedrodb/file/readonly_file.h"
#include "pedrodb/file/readwrite_file.h"
#include "pedrodb/format/index_format.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

namespace pedrodb {

class FileManager {
  mutable std::mutex mu_;

  MetadataManager* metadata_manager_;
  
  std::unordered_map<file_id_t, ReadableFile::Ptr> open_data_files_;
  const uint8_t max_open_files_;

  // always in use.
  std::shared_ptr<ArrayBuffer> active_index_log_;
  ReadWriteFile::Ptr active_data_file_;
  file_id_t active_file_id_{};

  Executor* io_executor_{};

  Status CreateFile(file_id_t id);

  Status Recovery(file_id_t active);

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

 public:
  FileManager(MetadataManager* metadata, Executor* executor,
              uint8_t max_open_files)
      : max_open_files_(max_open_files),
        io_executor_(executor),
        metadata_manager_(metadata) {}

  Status Init();

  Status Sync();

  Status Flush(bool force);

  template <typename Key, typename Value>
  Status WriteActiveFile(const record::Entry<Key, Value>& entry,
                         record::Location* loc) {
    for (auto lock = AcquireLock();;) {
      size_t offset = active_data_file_->Write(entry);
      if (offset != -1) {
        loc->offset = offset;
        loc->id = active_file_id_;

        index::Entry<Key> index_entry;
        index_entry.type = entry.type;
        index_entry.key = entry.key;
        index_entry.offset = offset;
        index_entry.len = entry.SizeOf();

        index_entry.Pack(active_index_log_.get());
        return Status::kOk;
      }

      auto status = CreateFile(active_file_id_ + 1);
      if (status != Status::kOk) {
        return status;
      }
    }
  }

  void ReleaseDataFile(file_id_t id);

  Status AcquireDataFile(file_id_t id, ReadableFile::Ptr* file);

  Status AcquireIndexFile(file_id_t id, ReadableFile::Ptr* file);

  Status RemoveFile(file_id_t id);
  
  void SyncActiveDataFile(file_id_t id, const WritableFile::Ptr& file);
  void CreateIndexFile(file_id_t id, const std::shared_ptr<ArrayBuffer>& log);
};
}  // namespace pedrodb
#endif  // PEDRODB_FILE_MANAGER_H
