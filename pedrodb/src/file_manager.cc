#include "pedrodb/file_manager.h"

namespace pedrodb {

Status FileManager::OpenActiveFile(WritableFileGuard* f, file_t id) {
  PEDRODB_INFO("create active file {}", id);
  WritableFile* file = nullptr;
  auto path = metadata_manager_->GetDataFilePath(id);
  auto status = WritableFile::Open(path, id, kMaxFileBytes, &file);
  if (status != Status::kOk) {
    PEDRODB_ERROR("failed to load active file: {}",
                  metadata_manager_->GetDataFilePath(id));
    return status;
  }
  f->reset(file);
  PEDRODB_INFO("open file: {}", metadata_manager_->GetDataFilePath(id));
  return status;
}

Status FileManager::Recovery(file_t active) {
  WritableFileGuard file;
  auto status = OpenActiveFile(&file, active);
  if (status != Status::kOk) {
    return status;
  }

  active_ = std::move(file);
  id_ = active;

  return Status::kOk;
}

Status FileManager::Init() {
  auto files = metadata_manager_->GetFiles();
  if (files.empty()) {
    auto status = CreateActiveFile();
    if (status != Status::kOk) {
      return status;
    }
    return Status::kOk;
  }

  return Recovery(files.back());
}

Status FileManager::CreateActiveFile() {
  WritableFileGuard active;
  file_t id = id_ + 1;

  auto stat = OpenActiveFile(&active, id);
  if (stat != Status::kOk) {
    return stat;
  }

  metadata_manager_->CreateFile(id);
  active_ = std::move(active);
  id_ = id;

  return Status::kOk;
}

void FileManager::ReleaseDataFile(file_t id) {
  auto lock = AcquireLock();
  auto pred = [id](const OpenFile& file) {
    return file.id == id;
  };
  auto it = std::remove_if(open_files_.begin(), open_files_.end(), pred);
  open_files_.erase(it, open_files_.end());
}

Status FileManager::AcquireDataFile(file_t id, ReadableFileGuard* file) {
  auto lock = AcquireLock();
  if (id == id_) {
    *file = active_;
    return Status::kOk;
  }

  // find the opened file with id
  auto pred = [id](const OpenFile& file) {
    return file.id == id;
  };
  auto it = std::find_if(open_files_.begin(), open_files_.end(), pred);
  if (it != open_files_.end()) {
    // lru strategy.
    auto open_file = std::move(*it);
    open_files_.erase(it);
    *file = open_files_.emplace_back(std::move(open_file)).file;
    return Status::kOk;
  }

  ReadonlyFile f;
  {
    lock.unlock();
    std::string filename = metadata_manager_->GetDataFilePath(id);
    auto stat = ReadonlyFile::Open(filename, &f);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
      return stat;
    }
    lock.lock();
  }
  *file = std::make_shared<ReadonlyFile>(std::move(f));

  OpenFile open_file{id, *file};
  if (open_files_.size() == max_open_files_) {
    open_files_.erase(open_files_.begin());
    open_files_.emplace_back(std::move(open_file));
  }

  return Status::kOk;
}

Error FileManager::RemoveDataFile(file_t id) {
  auto lock = AcquireLock();
  auto pred = [id](const OpenFile& file) {
    return file.id == id;
  };
  auto it = std::remove_if(open_files_.begin(), open_files_.end(), pred);
  open_files_.erase(it, open_files_.end());
  PEDRODB_IGNORE_ERROR(metadata_manager_->DeleteFile(id));
  return File::Remove(metadata_manager_->GetDataFilePath(id).c_str());
}

Status FileManager::WriteActiveFile(Buffer* buffer, record::Location* loc) {
  for (;;) {
    auto lock = AcquireLock();
    auto active = active_;
    lock.unlock();

    std::string_view view{buffer->ReadIndex(), buffer->ReadableBytes()};
    size_t offset = active->Write(view);
    if (offset != -1) {
      buffer->Retrieve(view.size());
      loc->offset = offset;
      loc->id = active->GetFile();
      return Status::kOk;
    }

    lock.lock();
    if (active != active_) {
      continue;
    }

    auto status = CreateActiveFile();
    if (status != Status::kOk) {
      return status;
    }
    lock.unlock();

    active->Sync();
  }
}

Status FileManager::Flush(bool force) {
  auto lock = AcquireLock();
  auto active = active_;
  lock.unlock();

  auto err = active->Flush(force);
  if (err != Error::kOk) {
    return Status::kIOError;
  }
  return Status::kOk;
}

Status FileManager::Sync() {
  std::unique_lock lock{mu_};
  auto active = active_;
  lock.unlock();

  if (auto err = active->Flush(true); err != Error::kOk) {
    return Status::kIOError;
  }

  if (auto err = active->Sync(); err != Error::kOk) {
    return Status::kIOError;
  }
  return Status::kOk;
}
}  // namespace pedrodb