#ifndef PEDRODB_DB_IMPL_H
#define PEDRODB_DB_IMPL_H

#include "pedrodb/db.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/random_access_file.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/file_manager.h"
#include "pedrodb/header.h"
#include "pedrodb/iterator/record_iterator.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata.h"
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace pedrodb {

struct ValueLocation {
  uint16_t file_id;
  uint32_t length;
  uint64_t offset;
  uint64_t timestamp;
};

class DBImpl : public DB {
  std::unordered_map<std::string, ValueLocation> value_indices_;
  std::mutex mu_;

  enum class State {
    kOpening,
    kOpened,
  };

  State state_{State::kOpening};

  std::unique_ptr<FileManager> file_manager_;
  std::unique_ptr<MetadataManager> metadata_;

  friend class DB;

public:
  ~DBImpl() override = default;

  DBImpl() : state_(State::kOpening) {}

  Status Init() {
    Status status = file_manager_->Init();
    if (status != Status::kOk) {
      return status;
    }

    PEDRODB_INFO("crash recover start");
    uint32_t active_id = metadata_->GetActiveID();
    for (uint16_t i = 1; i <= active_id; ++i) {
      auto file = file_manager_->GetFile(i);
      if (file == nullptr) {
        continue;
      }
      auto iter = RecordIterator(file);
      while (iter.Valid()) {
        auto record = iter.Next();
        std::string key{record.key};
        // TODO file_id cast to uint32_t ?
        if (record.type == Header::kSet) {
          value_indices_.emplace(std::move(key),
                                 ValueLocation{
                                     .file_id = i,
                                     .length = record.length,
                                     .offset = record.offset,
                                     .timestamp = record.timestamp,
                                 });
        } else if (record.type == Header::kDelete) {
          value_indices_.erase(key);
        }
      }
    }
    file_manager_->Close(active_id);
    PEDRODB_INFO("crash recover finished");

    state_ = State::kOpened;
    return Status::kOk;
  }

  Status Get(const ReadOptions &options, const std::string &key,
             std::string *value) override {
    std::unique_lock<std::mutex> lock(mu_);

    if (state_ != State::kOpened) {
      return Status::kIOError;
    }

    auto it = value_indices_.find(key);
    if (it == value_indices_.end()) {
      return Status::kNotFound;
    }

    auto location = it->second;
    auto file = file_manager_->GetFile(location.file_id);
    if (file == nullptr) {
      PEDRODB_ERROR("cannot get file {}", location.file_id);
      return Status::kIOError;
    }

    auto slice = file->Slice(location.offset, location.length);

    Header header{};
    if (!Header::Unpack(&header, &slice)) {
      PEDRODB_ERROR("failed to unpack header");
      return Status::kIOError;
    }
    
    std::string key0(header.key_size, 0);
    slice.Retrieve(key0.data(), key0.size());
    if (key0 != key) {
      PEDRODB_ERROR("key is not match: expected[{}], actual[{}]", key, key0);
      return Status::kCorruption;
    }
    
    if (header.timestamp != location.timestamp) {
      PEDRODB_ERROR("timestamp is not consistent: expect[{}], actual[{}]",
                    location.timestamp, header.timestamp);
      return Status::kCorruption;
    }

    if (header.type == Header::kSet) {
      value->resize(header.value_size, 0);
      slice.Retrieve(value->data(), header.value_size);
    }

    if (header.type == Header::kDelete) {
      return Status::kNotFound;
    }

    return Status::kOk;
  }

  Status Put(const WriteOptions &options, const std::string &key,
             std::string_view value) override {
    std::unique_lock<std::mutex> lock(mu_);
    if (state_ != State::kOpened) {
      return Status::kIOError;
    }

    size_t record_size = Header::PackedSize() + key.size() + value.size();
    pedrolib::ArrayBuffer buffer(record_size);

    uint64_t timestamp = metadata_->AddVersion();
    Header header{};
    header.key_size = key.size();
    header.type = Header::kSet;
    header.value_size = value.size();
    header.timestamp = timestamp;
    header.crc32 = 0;

    ValueLocation location{};
    location.timestamp = timestamp;
    location.length = record_size;
    if (location.length > kMaxFileBytes) {
      PEDRODB_ERROR("key or value is too big");
      return Status::kNotSupported;
    }

    auto active = file_manager_->GetActiveFile();
    // TODO file lock
    if (active->GetOffSet() + location.length > kMaxFileBytes) {
      auto err = file_manager_->CreateActiveFile();
      if (!err.Empty()) {
        PEDRODB_ERROR("failed to create active file");
        return Status::kIOError;
      }
      active = file_manager_->GetActiveFile();
    }

    location.file_id = active->GetID();
    location.offset = active->GetOffSet();

    header.Pack(&buffer);
    buffer.Append(key.data(), key.size());
    buffer.Append(value.data(), value.size());

    size_t expected = buffer.ReadableBytes();
    ssize_t w = active->Write(&buffer);
    if (w != location.length) {
      if (w < 0) {
        PEDRODB_ERROR("failed to write record: {}", Error{errno});
      } else {
        PEDRODB_ERROR("failed to write record: write[{}], expected[{}]", w,
                      expected);
      }
      return Status::kIOError;
    }

    if (options.sync) {
      auto err = active->Flush();
      if (!err.Empty()) {
        PEDRODB_ERROR("failed to flush data");
        return Status::kIOError;
      }
    }

    auto err = metadata_->Flush();
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to flush metadata");
      return Status::kIOError;
    }

    value_indices_[key] = location;
    return Status::kOk;
  }

  Status Delete(const WriteOptions &options, const std::string &key) override {
    std::unique_lock<std::mutex> lock(mu_);
    if (state_ != State::kOpened) {
      return Status::kIOError;
    }

    auto iter = value_indices_.find(key);
    if (iter == value_indices_.end()) {
      return Status::kNotFound;
    }

    size_t record_size = Header::PackedSize() + key.size();
    pedrolib::ArrayBuffer buffer(record_size);

    uint32_t timestamp = metadata_->AddVersion();
    Header header{};
    header.key_size = key.size();
    header.type = Header::kDelete;
    header.value_size = 0;
    header.timestamp = timestamp;
    header.crc32 = 0;

    ValueLocation location{};
    location.timestamp = timestamp;
    location.length = record_size;
    if (location.length > kMaxFileBytes) {
      return Status::kNotSupported;
    }

    auto active = file_manager_->GetActiveFile();
    // TODO file lock
    if (active->GetOffSet() + location.length > kMaxFileBytes) {
      auto err = file_manager_->CreateActiveFile();
      if (!err.Empty()) {
        return Status::kIOError;
      }
      active = file_manager_->GetActiveFile();
    }

    location.file_id = active->GetID();
    location.offset = active->GetOffSet();

    header.Pack(&buffer);
    buffer.Append(key.data(), key.size());

    if (active->Write(&buffer) != location.length) {
      return Status::kIOError;
    }

    if (options.sync) {
      auto err = active->Flush();
      if (!err.Empty()) {
        PEDRODB_ERROR("failed to flush data");
        return Status::kIOError;
      }
    }

    auto err = metadata_->Flush();
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to flush metadata");
      return Status::kIOError;
    }

    value_indices_.erase(iter);
    return Status::kOk;
  }
};
} // namespace pedrodb

#endif // PEDRODB_DB_IMPL_H
