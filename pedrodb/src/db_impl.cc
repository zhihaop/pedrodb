#include <utility>

#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions& options, std::string_view key,
                   std::string* value) {
  return HandleGet(options, Hash(key), key, value);
}

Status DBImpl::Put(const WriteOptions& options, std::string_view key,
                   std::string_view value) {
  return HandlePut(options, Hash(key), key, value);
}

Status DBImpl::Delete(const WriteOptions& options, std::string_view key) {
  return HandlePut(options, Hash(key), key, {});
}

void DBImpl::UpdateUnused(file_t id, size_t unused) {
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

  status = file_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  status = Recovery();
  if (status != Status::kOk) {
    return status;
  }

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

std::vector<file_t> DBImpl::GetFiles() {
  return metadata_manager_->GetFiles();
}

std::vector<file_t> DBImpl::PollCompactTask() {
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
  file_manager_ = std::make_unique<FileManager>(metadata_manager_.get(),
                                                options.max_open_files);
}

DBImpl::~DBImpl() {
  executor_->ScheduleCancel(sync_worker_);
  executor_->ScheduleCancel(compact_worker_);
  Flush();
}

Status DBImpl::Recovery(file_t id) {
  ReadableFileGuard file;
  auto status = file_manager_->AcquireDataFile(id, &file);
  if (status != Status::kOk) {
    return status;
  }

  ArrayBuffer buffer(kMaxFileBytes);
  auto iter = RecordIterator(file.get(), &buffer);
  while (iter.Valid()) {
    record::Location loc{id, iter.GetOffset()};
    PEDRODB_IGNORE_ERROR(Recovery(loc, iter.Next()));
  }

  PEDRODB_TRACE("crash recover success: file[{}], record[{}]", id,
                indices_.size());
  file_manager_->ReleaseDataFile(id);
  return Status::kOk;
}

Status DBImpl::CompactBatch(file_t id, const std::vector<Record>& records) {
  auto lock = AcquireLock();
  compact_hints_[id].state = CompactState::kCompacting;

  for (auto& r : records) {
    auto it = GetMetadataIterator(r.h, r.key);
    if (it == indices_.end()) {
      continue;
    }

    // steal entry.
    if (it->loc != r.location) {
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
    read_cache_->Remove(it->loc);

    record::Location loc;
    auto status = file_manager_->WriteActiveFile(entry, &loc);
    if (status != Status::kOk) {
      return status;
    }

    it->loc = loc;
    it->size = entry.SizeOf();
  }
  return Status::kOk;
}

void DBImpl::Compact(file_t id) {
  ReadableFileGuard file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return;
  }

  PEDRODB_INFO("start compacting {}", id);

  std::vector<Record> batch;
  size_t batch_bytes = 0;

  ArrayBuffer buffer;
  for (auto iter = RecordIterator(file.get(), &buffer); iter.Valid();) {
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
  PEDRODB_IGNORE_ERROR(file_manager_->RemoveDataFile(id));

  PEDRODB_TRACE("end compacting: {}", id);
}

Status DBImpl::HandlePut(const WriteOptions& options, uint32_t h,
                         std::string_view key, std::string_view value) {
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
  auto it = GetMetadataIterator(h, key);

  // check valid deletion.
  if (it == indices_.end() && value.empty()) {
    return Status::kNotFound;
  }

  // replace or delete.
  if (it != indices_.end()) {
    // remove cache.
    read_cache_->Remove(it->loc);

    // update compact hints.
    UpdateUnused(it->loc.id, it->size);

    if (value.empty()) {
      // delete.
      indices_.erase(it);
    } else {
      // replace.
      it->loc = loc;
      it->size = entry.SizeOf();
    }
    return Status::kOk;
  }

  // insert.
  indices_.emplace(h, std::string{key}, loc, entry.SizeOf());
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

auto DBImpl::GetMetadataIterator(uint32_t h, std::string_view key)
    -> decltype(indices_.begin()) {
  auto [s, t] = indices_.equal_range(record::Dir{h});
  auto it = std::find_if(s, t, [=](auto& k) { return k.CompareKey(key) == 0; });
  return it == t ? indices_.end() : it;
}

Status DBImpl::HandleGet(const ReadOptions& options, uint32_t h,
                         std::string_view key, std::string* value) {
  record::Location loc;
  size_t size;
  {
    auto lock = AcquireLock();
    auto it = GetMetadataIterator(h, key);
    if (it == indices_.end()) {
      return Status::kNotFound;
    }
    loc = it->loc;
    size = it->size;
  }

  if (read_cache_->Read(loc, value)) {
    return Status::kOk;
  }

  ReadableFileGuard file;
  auto stat = file_manager_->AcquireDataFile(loc.id, &file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot get file {}", loc.id);
    return stat;
  }

  return FetchRecord(file.get(), loc, size, value);
}

Status DBImpl::Recovery(record::Location loc, record::EntryView entry) {
  uint32_t h = Hash(entry.key);
  auto it = GetMetadataIterator(h, entry.key);

  if (entry.type == record::Type::kSet) {
    if (it == indices_.end()) {
      indices_.emplace(h, std::string{entry.key}, loc, entry.SizeOf());
      return Status::kOk;
    }

    // indices has the newer version data.
    if (it->loc > loc) {
      UpdateUnused(loc.id, entry.SizeOf());
      return Status::kOk;
    }

    // never happen.
    if (it->loc == loc) {
      PEDRODB_FATAL("meta.loc == loc should never happened");
    }

    // indices has the elder version data.
    UpdateUnused(it->loc.id, it->size);

    // update indices.
    it->loc = loc;
    it->size = entry.SizeOf();
  }

  // a tombstone of deletion.
  if (entry.type == record::Type::kDelete) {
    UpdateUnused(loc.id, entry.SizeOf());
    if (it == indices_.end()) {
      return Status::kOk;
    }

    // Recover(file_t) should be called monotonously,
    // therefore entry.loc is always monotonously increased.
    // should not delete the latest version data.
    if (it->loc > loc) {
      return Status::kOk;
    }

    UpdateUnused(it->loc.id, it->size);
    indices_.erase(it);
  }

  return Status::kOk;
}

Record::Record(uint32_t h, std::string key, std::string value,
               const record::Location& location, uint32_t timestamp)
    : h(h),
      key(std::move(key)),
      value(std::move(value)),
      location(location),
      timestamp(timestamp) {}
}  // namespace pedrodb