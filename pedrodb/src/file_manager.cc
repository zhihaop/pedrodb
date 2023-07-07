#include "pedrodb/file_manager.h"

namespace pedrodb {

Status FileManager::OpenActiveFile(WritableFileGuard *file, file_t id) {
  PEDRODB_INFO("create active file {}", id);
  auto path = metadata_manager_->GetDataFilePath(id);
  auto status = WritableFile::OpenOrCreate(path, file);
  if (status != Status::kOk) {
    PEDRODB_ERROR("failed to load active file: {}",
                  metadata_manager_->GetDataFilePath(id));
    return status;
  }
  PEDRODB_INFO("create active file: {}",
               metadata_manager_->GetDataFilePath(id));
  return status;
}

Status FileManager::Recovery(file_t active) {
  WritableFileGuard file;
  auto status = OpenActiveFile(&file, active);
  if (status != Status::kOk) {
    return status;
  }

  std::string value;
  auto err = file->ReadAll(&value);
  if (!err.Empty()) {
    return Status::kIOError;
  }

  active_ = std::move(file);
  memtable_.Update(active, std::move(value));
  latest_ = {
      .id = active,
      .offset = (uint32_t)memtable_.Size(),
  };

  return Status::kOk;
}

Status FileManager::Init() {
  auto &files = metadata_manager_->GetFiles();
  if (files.empty()) {
    auto status = CreateActiveFile();
    if (status != Status::kOk) {
      return status;
    }
    return Status::kOk;
  }

  return Recovery(*std::max_element(files.begin(), files.end()));
}

Status FileManager::CreateActiveFile() {
  WritableFileGuard active;
  file_t id = latest_.id + 1;

  auto stat = OpenActiveFile(&active, id);
  if (stat != Status::kOk) {
    return stat;
  }

  auto _ = metadata_manager_->AcquireLock();
  metadata_manager_->CreateFile(id);
  active_ = std::move(active);
  latest_.id = id;
  latest_.offset = 0;
  memtable_.Update(id);

  return Status::kOk;
}

Status FileManager::ReleaseDataFile(file_t id) {
  if (id == latest_.id) {
    return Status::kOk;
  }
  
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::remove_if(open_files_.begin(), open_files_.end(), pred);
  open_files_.erase(it, open_files_.end());
  return Status::kOk;
}

Status FileManager::AcquireDataFile(file_t id, ReadableFileGuard *file) {
  if (id == latest_.id) {
    *file = std::make_shared<MemoryTableView>(&memtable_, this);
    return Status::kOk;
  }

  // find the opened file with id
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::find_if(open_files_.begin(), open_files_.end(), pred);
  if (it != open_files_.end()) {
    // lru strategy.
    auto open_file = std::move(*it);
    open_files_.erase(it);
    *file = open_files_.emplace_back(std::move(open_file)).file;
    return Status::kOk;
  }

  ReadableFileImpl f;
  std::string filename = metadata_manager_->GetDataFilePath(id);
  auto stat = ReadableFileImpl::Open(filename, &f);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }
  *file = std::make_shared<ReadableFileImpl>(std::move(f));

  OpenFile open_file{id, *file};
  if (open_files_.size() == max_open_files_) {
    open_files_.erase(open_files_.begin());
    open_files_.emplace_back(std::move(open_file));
  }

  return stat;
}

void MemoryTableView::UpdateFile() const noexcept {
  if (view_->GetID() == id_) {
    return;
  }
  view_ = nullptr;

  auto lock = parent_->AcquireLock();
  auto status = parent_->AcquireDataFile(id_, &file_);
  if (status != Status::kOk) {
    return;
  }
}
} // namespace pedrodb