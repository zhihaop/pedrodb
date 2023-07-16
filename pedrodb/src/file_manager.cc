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

void SyncRunner(Executor* executor, file_id_t id, WritableFileGuard file) {
  auto err = file->Sync();
  if (err != Error::kOk) {
    PEDRODB_WARN("failed to sync active file to disk");
    executor->ScheduleAfter(Duration::Seconds(1), [executor, id, file] {
      SyncRunner(executor, id, file);
    });
  }
  PEDRODB_INFO("flush file {} success", id);
}

Status FileManager::CreateFile(file_id_t id) {

  if (active_data_file_) {
    PEDRODB_IGNORE_ERROR(active_data_file_->Flush(true));
    PEDRODB_IGNORE_ERROR(active_index_file_->Flush(true));
    
    io_executor_->Schedule(
        [=] { SyncRunner(io_executor_, active_file_id_, active_data_file_); });

    io_executor_->Schedule(
        [=] { SyncRunner(io_executor_, active_file_id_, active_index_file_); });
  }

  WritableFileGuard data_file;
  WritableFileGuard index_file;

  auto data_file_path = metadata_manager_->GetDataFilePath(id);
  auto index_file_path = metadata_manager_->GetIndexFilePath(id);

  auto err = WritableFile::Open(data_file_path, kMaxFileBytes, &data_file);
  if (err != Status::kOk) {
    return err;
  }

  err = WritableFile::Open(index_file_path, kMaxFileBytes, &index_file);
  if (err != Status::kOk) {
    return err;
  }

  metadata_manager_->CreateFile(id);
  active_data_file_ = data_file;
  active_index_file_ = index_file;
  active_file_id_ = id;

  return Status::kOk;
}

void FileManager::ReleaseDataFile(file_id_t id) {
  auto lock = AcquireLock();
  open_data_files_.erase(id);
}

Status FileManager::AcquireDataFile(file_id_t id, ReadableFileGuard* file) {
  auto lock = AcquireLock();
  if (id == active_file_id_) {
    *file = active_data_file_;
    return Status::kOk;
  }

  auto it = open_data_files_.find(id);
  if (it != open_data_files_.end()) {
    *file = it->second;
    return Status::kOk;
  }

  lock.unlock();
  std::string filename = metadata_manager_->GetDataFilePath(id);
  auto stat = ReadonlyFile::Open(filename, file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }
  lock.lock();

  if (open_data_files_.size() == max_open_files_) {
    open_data_files_.erase(open_data_files_.begin());
  }
  open_data_files_.emplace(id, *file);

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

Status FileManager::AcquireIndexFile(file_id_t id, ReadableFileGuard* file) {
  std::string filename = metadata_manager_->GetIndexFilePath(id);
  auto stat = ReadonlyFile::Open(filename, file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }
  return Status::kOk;
}
}  // namespace pedrodb