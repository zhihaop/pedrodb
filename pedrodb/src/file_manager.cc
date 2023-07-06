#include "pedrodb/file_manager.h"

namespace pedrodb {

Status FileManager::OpenActiveFile(WritableFile **file, file_t id) {
  PEDRODB_INFO("create active file {}", id);
  auto status =
      WritableFile::OpenOrCreate(metadata_manager_->GetDataFilePath(id), file);
  if (status != Status::kOk) {
    PEDRODB_ERROR("failed to load active file: {}",
                  metadata_manager_->GetDataFilePath(id));
    return status;
  }
  PEDRODB_INFO("create active file: {}",
               metadata_manager_->GetDataFilePath(id));
  return status;
}

Status FileManager::Init() {
  auto &files = metadata_manager_->GetFiles();
  if (files.empty()) {
    active_ = nullptr;
    active_id_ = 0;
    auto status = CreateActiveFile();
    if (status != Status::kOk) {
      return status;
    }
    read_cache_->UpdateActiveID(active_id_);
    return Status::kOk;
  }

  active_id_ = *std::max_element(files.begin(), files.end());
  read_cache_->UpdateActiveID(active_id_);

  WritableFile *file;
  auto status = OpenActiveFile(&file, active_id_);
  if (status != Status::kOk) {
    return status;
  }
  active_.reset(file);

  return Status::kOk;
}

Status FileManager::CreateActiveFile() {
  WritableFile *active = nullptr;
  file_t id = active_id_ + 1;

  auto stat = OpenActiveFile(&active, id);
  if (stat != Status::kOk) {
    return stat;
  }

  auto _ = metadata_manager_->AcquireLock();
  metadata_manager_->CreateFile(id);
  active_.reset(active);
  active_id_ = id;

  return Status::kOk;
}

Status FileManager::ReleaseDataFile(file_t id) {
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::remove_if(open_files_.begin(), open_files_.end(), pred);
  open_files_.erase(it, open_files_.end());
  return Status::kOk;
}

Status FileManager::AcquireDataFile(file_t id, ReadableFileGuard *file) {
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::find_if(open_files_.begin(), open_files_.end(), pred);
  if (it != open_files_.end()) {
    auto open_file = std::move(*it);
    open_files_.erase(it);
    *file = open_files_.emplace_back(std::move(open_file)).file;
    return Status::kOk;
  }

  ReadableFile f;
  std::string filename = metadata_manager_->GetDataFilePath(id);
  auto stat = ReadableFile::Open(filename, &f);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }
  *file = std::make_shared<ReadableFile>(std::move(f));

  OpenFile open_file{id, *file};
  if (open_files_.size() == max_open_files_) {
    open_files_.erase(open_files_.begin());
    open_files_.emplace_back(std::move(open_file));
  }

  return stat;
}
} // namespace pedrodb