#include "pedrodb/file_manager.h"

namespace pedrodb {

Status FileManager::Recovery(file_id_t id) {
  auto status = CreateFile(id);
  if (status != Status::kOk) {
    return status;
  }
  return Status::kOk;
}

Status FileManager::Init() {
  auto files = metadata_manager_->GetFiles();
  if (files.empty()) {
    auto status = CreateFile(1);
    if (status != Status::kOk) {
      return status;
    }
    return Status::kOk;
  }

  return Recovery(files.back());
}

void FileManager::SyncFile(file_id_t id, const WritableFile::Ptr& file) {
  auto err = file->Sync();
  if (err != Error::kOk) {
    PEDRODB_WARN("failed to sync active file to disk");
    executor_->ScheduleAfter(
        Duration::Seconds(1),
        [this, self = shared_from_this(), id, file] { SyncFile(id, file); });
    return;
  }
  PEDRODB_TRACE("sync file {} success", id);
}

void FileManager::CreateIndexFile(file_id_t id,
                                  const std::shared_ptr<ArrayBuffer>& log) {
  auto index_path = metadata_manager_->GetIndexFilePath(id);
  ReadWriteFile::Ptr file;
  auto status = ReadWriteFile::Open(index_path, log->ReadableBytes(), &file);
  if (status != Status::kOk) {
    executor_->ScheduleAfter(Duration::Seconds(1),
                             [this, self = shared_from_this(), id, log] {
                               CreateIndexFile(id, log);
                             });
    return;
  }

  file->Append(log->ReadIndex(), log->ReadableBytes());
  PEDRODB_TRACE("create index file {} success", id);
}

Status FileManager::CreateFile(file_id_t id) {
  if (active_data_file_) {
    PEDRODB_TRACE("flush {} to disk", id);
    PEDRODB_IGNORE_ERROR(active_data_file_->Flush(true));

    executor_->Schedule([this, self = shared_from_this(), id = active_file_id_,
                         f = active_data_file_] { SyncFile(id, f); });

    auto log = std::move(active_index_log_);
    if (log != nullptr) {
      executor_->Schedule([this, self = shared_from_this(),
                           id = active_file_id_,
                           log] { CreateIndexFile(id, log); });
    }
  }

  ReadWriteFile::Ptr data_file;

  auto data_file_path = metadata_manager_->GetDataFilePath(id);
  auto err = ReadWriteFile::Open(data_file_path, kMaxFileBytes, &data_file);
  if (err != Status::kOk) {
    return err;
  }

  // Rebuild index from file.
  record::EntryView entry;
  uint32_t offset = 0;
  active_index_log_ = std::make_shared<ArrayBuffer>();

  auto buffer = data_file->GetReadonlyBuffer();
  while (entry.UnPack(&buffer)) {
    index::EntryView index;
    index.offset = offset;
    index.len = entry.SizeOf();
    index.type = entry.type;
    index.key = entry.key;

    offset += entry.SizeOf();
    index.Pack(active_index_log_.get());
  }

  if (offset != 0) {
    PEDRODB_WARN("last offset {}", offset);
    data_file->SetWriteIndex(offset);
  }

  metadata_manager_->CreateFile(id);
  active_data_file_ = data_file;
  active_file_id_ = id;

  return Status::kOk;
}

void FileManager::ReleaseDataFile(file_id_t id) {
  auto lock = AcquireLock();
  ReadableFile::Ptr file;
  open_files_.Remove(id, file);
}

Status FileManager::AcquireDataFile(file_id_t id, ReadableFile::Ptr* file) {
  auto lock = AcquireLock();
  if (id == active_file_id_) {
    *file = active_data_file_;
    return Status::kOk;
  }

  if (open_files_.Get(id, *file)) {
    return Status::kOk;
  }

  lock.unlock();
  std::string filename = metadata_manager_->GetDataFilePath(id);
  auto stat = PosixReadonlyFile::Open(filename, file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }
  lock.lock();

  open_files_.Put(id, *file);
  return Status::kOk;
}

Status FileManager::RemoveFile(file_id_t id) {
  ReleaseDataFile(id);

  auto data_file_path = metadata_manager_->GetDataFilePath(id);
  auto index_file_path = metadata_manager_->GetIndexFilePath(id);
  auto status = metadata_manager_->DeleteFile(id);
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_IGNORE_ERROR(File::Remove(data_file_path.c_str()));
  PEDRODB_IGNORE_ERROR(File::Remove(index_file_path.c_str()));
  return Status::kOk;
}

Status FileManager::Flush(bool force) {
  auto lock = AcquireLock();
  auto active = active_data_file_;
  lock.unlock();

  if (active == nullptr) {
    return Status::kOk;
  }

  auto err = active->Flush(force);
  if (err != Error::kOk) {
    return Status::kIOError;
  }
  return Status::kOk;
}

Status FileManager::Sync() {
  std::unique_lock lock{mu_};
  auto active = active_data_file_;
  lock.unlock();

  if (active == nullptr) {
    return Status::kOk;
  }

  if (auto err = active->Flush(true); err != Error::kOk) {
    return Status::kIOError;
  }

  if (auto err = active->Sync(); err != Error::kOk) {
    return Status::kIOError;
  }
  return Status::kOk;
}

Status FileManager::AcquireIndexFile(file_id_t id, ReadableFile::Ptr* file) {
  return MappingReadonlyFile::Open(metadata_manager_->GetIndexFilePath(id),
                                   file);
}
}  // namespace pedrodb