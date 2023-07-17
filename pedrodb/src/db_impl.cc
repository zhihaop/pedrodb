#include <utility>

#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions& options, std::string_view key,
                   std::string* value) {
  thread_local std::string buffer;
  buffer.assign(key);
  return HandleGet(options, buffer, value);
}

Status DBImpl::Put(const WriteOptions& options, std::string_view key,
                   std::string_view value) {
  thread_local std::string buffer;
  buffer.assign(key);
  return HandlePut(options, buffer, value);
}

Status DBImpl::Delete(const WriteOptions& options, std::string_view key) {
  thread_local std::string buffer;
  buffer.assign(key);
  return HandlePut(options, buffer, {});
}

void DBImpl::UpdateUnused(file_id_t id, size_t unused) {
  auto& hint = compact_hints_[id];
  hint.unused += unused;
  if (hint.unused >= options_.compaction_threshold_bytes) {
    if (hint.state == CompactState::kNop) {
      compact_tasks_.emplace(id);
    }
  }
}

Status DBImpl::Init() {
  Status status = metadata_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_INFO("metadata manager init success");
  status = file_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_INFO("file manager init success");
  status = Recovery();
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_INFO("recovery success");

  sync_worker_ = executor_->ScheduleEvery(
      options_.sync_interval, options_.sync_interval, [this] { Flush(); });

  compact_worker_ = executor_->ScheduleEvery(
      options_.compact_interval, options_.compact_interval, [this] {
        auto lock = AcquireLock();
        auto task = PollCompactTask();
        lock.unlock();

        std::for_each(task.begin(), task.end(), [this](auto task) {
          executor_->Schedule([this, task] { Compact(task); });
        });
      });

  return Status::kOk;
}

std::vector<file_id_t> DBImpl::GetFiles() {
  return metadata_manager_->GetFiles();
}

std::vector<file_id_t> DBImpl::PollCompactTask() {
  auto tasks = compact_tasks_;
  compact_tasks_.clear();
  for (auto file : tasks) {
    compact_hints_[file].state = CompactState::kScheduling;
  }
  return {tasks.begin(), tasks.end()};
}

Status DBImpl::Compact() {
  auto lock = AcquireLock();
  auto tasks = PollCompactTask();
  lock.unlock();

  std::sort(tasks.begin(), tasks.end());
  for (auto file : tasks) {
    Compact(file);
  }

  return Status::kOk;
}

DBImpl::DBImpl(const Options& options, const std::string& name)
    : options_(options) {
  read_cache_ = std::make_unique<ReadCache>(options_.read_cache_bytes);
  executor_ = options_.executor;
  metadata_manager_ = std::make_unique<MetadataManager>(name);
  file_manager_ = std::make_unique<FileManager>(
      metadata_manager_.get(), executor_.get(), options.max_open_files);
}

DBImpl::~DBImpl() {
  executor_->ScheduleCancel(sync_worker_);
  executor_->ScheduleCancel(compact_worker_);
  Flush();
}

Status DBImpl::Recovery(file_id_t id) {
  ReadableFile::Ptr file;
  auto status = file_manager_->AcquireIndexFile(id, &file);
  if (status != Status::kOk) {
    return status;
  }

  auto iter = IndexIterator(file.get());
  while (iter.Valid()) {
    Recovery(id, iter.Next());
  }

  PEDRODB_INFO("crash recover success: file[{}], record[{}]", id,
               indices_.size());
  file_manager_->ReleaseDataFile(id);
  return Status::kOk;
}

Status DBImpl::CompactBatch(file_id_t id, const std::vector<Record>& records) {
  auto lock = AcquireLock();
  compact_hints_[id].state = CompactState::kCompacting;

  for (auto& r : records) {
    auto it = indices_.find(r.key);
    if (it == indices_.end()) {
      continue;
    }

    // steal entry.
    auto& dir = it->second;
    if (dir.loc != r.location) {
      continue;
    }

    // move to active file because this file will be removed.
    record::EntryView entry;
    entry.crc32 = 0;
    entry.type = record::Type::kSet;
    entry.key = r.key;
    entry.value = r.value;
    entry.timestamp = r.timestamp;

    // remove cache.
    read_cache_->Remove(dir.loc);

    record::Location loc;
    auto status = file_manager_->WriteActiveFile(entry, &loc);
    if (status != Status::kOk) {
      return status;
    }

    dir.loc = loc;
    dir.entry_size = entry.SizeOf();
  }
  return Status::kOk;
}

void DBImpl::Compact(file_id_t id) {
  ReadableFile::Ptr file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return;
  }

  PEDRODB_INFO("start compacting {}", id);

  std::vector<Record> batch;
  size_t batch_bytes = 0;

  for (auto iter = RecordIterator(file.get()); iter.Valid();) {
    uint32_t offset = iter.GetOffset();
    auto next = iter.Next();
    if (next.type != record::Type::kSet) {
      continue;
    }
    batch_bytes += next.SizeOf();
    batch.emplace_back(Hash(next.key), std::string{next.key},
                       std::string{next.value}, record::Location{id, offset},
                       (uint32_t)0);

    if (batch_bytes >= options_.compaction_batch_bytes) {
      CompactBatch(id, batch);
      batch.clear();
      batch_bytes = 0;
    }
  }

  if (batch_bytes > 0) {
    CompactBatch(id, batch);
    batch.clear();
  }

  // erase compaction_state.
  {
    auto lock = AcquireLock();
    compact_hints_.erase(id);
    compact_tasks_.erase(id);
  }

  // remove file.
  PEDRODB_IGNORE_ERROR(file_manager_->RemoveFile(id));

  PEDRODB_TRACE("end compacting: {}", id);
}

Status DBImpl::HandlePut(const WriteOptions& options, const std::string& key,
                         std::string_view value) {
  record::EntryView entry;
  entry.crc32 = 0;
  entry.type = value.empty() ? record::Type::kDelete : record::Type::kSet;
  entry.key = key;
  entry.value = value;

  if (entry.SizeOf() > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  uint32_t timestamp = 0;
  entry.timestamp = timestamp;

  record::Location loc{};
  auto status = file_manager_->WriteActiveFile(entry, &loc);
  if (status != Status::kOk) {
    return status;
  }

  auto lock = AcquireLock();
  {
    auto it = indices_.find(key);

    // check valid deletion.
    if (it == indices_.end() && value.empty()) {
      return Status::kNotFound;
    }

    // replace or delete.
    if (it != indices_.end()) {
      auto& dir = it->second;
      // remove cache.
      read_cache_->Remove(dir.loc);

      // update compact hints.
      UpdateUnused(dir.loc.id, dir.entry_size);

      if (value.empty()) {
        // delete.
        indices_.erase(it);
      } else {
        // replace.
        dir.loc = loc;
        dir.entry_size = entry.SizeOf();
      }
      return Status::kOk;
    }

    // insert.
    auto& dir = indices_[key];
    dir.loc = loc;
    dir.entry_size = entry.SizeOf();
  }

  if (options.sync) {
    file_manager_->Sync();
  }

  return Status::kOk;
}

Status DBImpl::Flush() {
  return file_manager_->Flush(true);
}

Status DBImpl::FetchRecord(ReadableFile* file, const record::Location& loc,
                           size_t size, std::string* value) {

  ArrayBuffer buffer(size);
  ssize_t r = file->Read(loc.offset, buffer.WriteIndex(), size);
  if (r != size) {
    PEDRODB_ERROR("failed to read from file {}, return {} expect {}, {}",
                  loc.id, r, size, file->GetError());
    return Status::kIOError;
  }
  buffer.Append(size);

  record::EntryView entry;
  if (!entry.UnPack(&buffer)) {
    return Status::kCorruption;
  }

  *value = entry.value;
  read_cache_->Put(loc, *value);
  return Status::kOk;
}

Status DBImpl::Recovery() {
  for (auto file : GetFiles()) {
    PEDRODB_TRACE("crash recover: file {}", file);
    auto status = Recovery(file);
    if (status != Status::kOk) {
      return status;
    }
  }
  return Status::kOk;
}

Status DBImpl::HandleGet(const ReadOptions& options, const std::string& key,
                         std::string* value) {

  record::Dir dir;
  {
    auto lock = AcquireLock();
    auto it = indices_.find(key);
    if (it == indices_.end()) {
      return Status::kNotFound;
    }
    dir = it->second;
  }

  if (read_cache_->Read(dir.loc, value)) {
    return Status::kOk;
  }

  ReadableFile::Ptr file;
  auto stat = file_manager_->AcquireDataFile(dir.loc.id, &file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot get file {}", dir.loc.id);
    return stat;
  }

  return FetchRecord(file.get(), dir.loc, dir.entry_size, value);
}

void DBImpl::Recovery(file_id_t id, index::EntryView entry) {
  record::Location loc(id, entry.offset);
  std::string key(entry.key);

  auto it = indices_.find(key);
  if (entry.type == record::Type::kSet) {
    if (it == indices_.end()) {
      auto& dir = indices_[key];
      dir.entry_size = entry.len;
      dir.loc = loc;
      return;
    }

    // indices has the newer version data.
    auto& dir = it->second;
    if (dir.loc > loc) {
      UpdateUnused(loc.id, entry.len);
      return;
    }

    // never happen.
    if (dir.loc == loc) {
      PEDRODB_FATAL("meta.loc == loc should never happened");
    }

    // indices has the elder version data.
    UpdateUnused(dir.loc.id, dir.entry_size);

    // update indices.
    dir.loc = loc;
    dir.entry_size = entry.len;
  }

  // a tombstone of deletion.
  if (entry.type == record::Type::kDelete) {
    UpdateUnused(loc.id, entry.len);
    if (it == indices_.end()) {
      return;
    }

    // Recover(file_id_t) should be called monotonously,
    // therefore entry.loc is always monotonously increased.
    // should not delete the latest version data.
    auto& dir = it->second;
    if (dir.loc > loc) {
      return;
    }

    UpdateUnused(dir.loc.id, dir.entry_size);
    indices_.erase(it);
  }
}

Record::Record(uint32_t h, std::string key, std::string value,
               const record::Location& location, uint32_t timestamp)
    : h(h),
      key(std::move(key)),
      value(std::move(value)),
      location(location),
      timestamp(timestamp) {}
}  // namespace pedrodb