#include <utility>

#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions &options, std::string_view key,
                   std::string *value) {
  auto lock = AcquireSharedLock();
  return HandleGet(options, Hash(key), key, value);
}

Status DBImpl::Put(const WriteOptions &options, std::string_view key,
                   std::string_view value) {
  auto lock = AcquireLock();
  return HandlePut(options, Hash(key), key, value);
}

Status DBImpl::Delete(const WriteOptions &options, std::string_view key) {
  auto lock = AcquireLock();
  return HandlePut(options, Hash(key), key, {});
}

void DBImpl::UpdateUnused(file_t id, size_t unused) {
  auto &hint = compact_hints_[id];
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

        std::for_each(task.begin(), task.end(), [=](auto task) {
          executor_->Schedule([=] { Compact(task); });
        });
      });

  return Status::kOk;
}

std::vector<file_t> DBImpl::GetFiles() {
  std::vector<file_t> files;
  {
    auto _ = metadata_manager_->AcquireLock();
    auto all_files = metadata_manager_->GetFiles();
    files.assign(all_files.begin(), all_files.end());
  }

  std::sort(files.begin(), files.end());
  return files;
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

DBImpl::DBImpl(const Options &options, const std::string &name)
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

  buffer_.Reset();
  auto iter = RecordIterator(file.get(), &buffer_);
  while (iter.Valid()) {
    PEDRODB_IGNORE_ERROR(Recovery(id, iter.Next()));
  }

  PEDRODB_TRACE("crash recover success: file[{}], record[{}]", id,
                indices_.size());
  file_manager_->ReleaseDataFile(id);
  return Status::kOk;
}

Status DBImpl::CompactBatch(file_t id, const std::vector<Record> &records) {
  ArrayBuffer buffer;
  auto lock = AcquireLock();
  compact_hints_[id].state = CompactState::kCompacting;

  for (auto &r : records) {
    auto it = GetMetadataIterator(r.h, r.key);
    if (it == indices_.end()) {
      continue;
    }

    // steal record.
    if (it->loc != r.location) {
      continue;
    }

    // move to active file because this file will be removed.
    buffer.Reset();
    record::Header header{
        (uint32_t)0,           record::Type::kSet,
        (uint8_t)r.key.size(), (uint32_t)r.value.size(),
        r.timestamp,
    };

    header.Pack(&buffer);
    buffer.Append(r.key.data(), r.key.size());
    buffer.Append(r.value.data(), r.value.size());
    // remove cache.
    read_cache_->Remove(it->loc);

    record::Location loc;
    auto _ = file_manager_->AcquireLock();
    auto status = file_manager_->WriteActiveFile(&buffer, &loc);
    if (status != Status::kOk) {
      return status;
    }
    
    it->loc = loc;
    it->size = record::SizeOf(r.key.size(), r.value.size());
  }
  return Status::kOk;
}

void DBImpl::Compact(file_t id) {
  ReadableFileGuard file;
  {
    auto _ = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(id, &file);
    if (stat != Status::kOk) {
      return;
    }
  }

  PEDRODB_INFO("start compacting {}", id);

  std::vector<Record> batch;
  size_t batch_bytes = 0;

  ArrayBuffer buffer;
  for (auto iter = RecordIterator(file.get(), &buffer); iter.Valid();) {
    auto next = iter.Next();
    if (next.type != record::Type::kSet) {
      continue;
    }
    batch_bytes += record::SizeOf(next.key.size(), next.value.size());
    batch.emplace_back(Hash(next.key), std::string{next.key},
                       std::string{next.value},
                       record::Location{id, next.offset}, (uint32_t)0);

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
  {
    auto _ = file_manager_->AcquireLock();
    file_manager_->ReleaseDataFile(id);
    PEDRODB_IGNORE_ERROR(file_manager_->RemoveDataFile(id));
  }

  PEDRODB_TRACE("end compacting: {}", id);
}

Status DBImpl::HandlePut(const WriteOptions &options, uint32_t h,
                         std::string_view key, std::string_view value) {

  uint32_t record_size = record::SizeOf(key.size(), value.size());
  if (record_size > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  // check valid deletion.
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end() && value.empty()) {
    return Status::kNotFound;
  }

  uint32_t timestamp = 0;

  record::Header header;
  header.crc32 = 0;
  header.type = value.empty() ? record::Type::kDelete : record::Type::kSet;
  header.key_size = key.size();
  header.value_size = value.size();
  header.timestamp = timestamp;

  buffer_.Reset();
  header.Pack(&buffer_);
  buffer_.Append(key.data(), key.size());
  buffer_.Append(value.data(), value.size());

  record::Location loc{};
  auto status = file_manager_->WriteActiveFile(&buffer_, &loc);
  if (status != Status::kOk) {
    return status;
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
      it->size = record_size;
    }
    return Status::kOk;
  }

  // insert.
  indices_.emplace(h, std::string{key}, loc, record_size);
  return Status::kOk;
}

Status DBImpl::Flush() {
  auto lock = file_manager_->AcquireLock();
  file_manager_->Sync();
  return Status::kOk;
}

Status DBImpl::FetchRecord(ReadableFile *file, const record::Dir &dir,
                           std::string *value) {
  record::Location l = dir.loc;

  ArrayBuffer buffer;
  buffer.Reset();
  buffer.EnsureWriteable(dir.size);
  ssize_t r = file->Read(l.offset, buffer.WriteIndex(), dir.size);
  if (r != dir.size) {
    PEDRODB_ERROR("failed to read from file {}, returns {}: {}", l.id, r,
                  file->GetError());
    return Status::kIOError;
  }
  buffer.Append(dir.size);

  record::Header header{};
  if (!header.UnPack(&buffer)) {
    return Status::kCorruption;
  }

  if (buffer.ReadableBytes() != header.key_size + header.value_size) {
    return Status::kCorruption;
  }

  buffer.Retrieve(header.key_size);
  value->resize(header.value_size);
  buffer.Retrieve(value->data(), value->size());
  read_cache_->Put(l, *value);
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
  auto it = std::find_if(s, t, [=](auto &k) { return k.key == key; });
  return it == t ? indices_.end() : it;
}

Status DBImpl::HandleGet(const ReadOptions &options, uint32_t h,
                         std::string_view key, std::string *value) {
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end()) {
    return Status::kNotFound;
  }

  if (read_cache_->Read(it->loc, value)) {
    return Status::kOk;
  }

  ReadableFileGuard file;
  {
    auto lock = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(it->loc.id, &file);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot get file {}", it->loc.id);
      return stat;
    }
  }

  return FetchRecord(file.get(), *it, value);
}

Status DBImpl::Recovery(file_t id, RecordEntry entry) {
  record::Location loc(id, entry.offset);
  uint32_t size = record::SizeOf(entry.key.size(), entry.value.size());

  uint32_t h = Hash(entry.key);
  auto it = GetMetadataIterator(h, entry.key);

  if (entry.type == record::Type::kSet) {
    if (it == indices_.end()) {
      indices_.emplace(h, std::string{entry.key}, loc, size);
      return Status::kOk;
    }

    // indices has the newer version data.
    if (it->loc > loc) {
      UpdateUnused(id, record::SizeOf(entry.key.size(), entry.value.size()));
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
    it->size = size;
  }

  // a tombstone of deletion.
  if (entry.type == record::Type::kDelete) {
    UpdateUnused(id, record::SizeOf(entry.key.size(), entry.value.size()));
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
               const record::Location &location, uint32_t timestamp)
    : h(h), key(std::move(key)), value(std::move(value)), location(location),
      timestamp(timestamp) {}
} // namespace pedrodb