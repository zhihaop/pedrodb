#ifndef PEDRODB_DB_IMPL_H
#define PEDRODB_DB_IMPL_H

#include "pedrodb/db.h"
#include <pedrolib/timestamp.h>
#include <unordered_map>

namespace pedrodb {

struct ValuePosition {
  uint16_t file_id;
  uint32_t record_size;
  uint32_t record_offset;
  Timestamp timestamp;
};

class DBImpl : public DB {
  std::unordered_map<std::string, ValuePosition> value_indices_;

public:
  ~DBImpl() override = default;
  Status Get(const ReadOptions &options, std::string_view key,
             std::string *value) override {
    return Status::kCorruption;
  }
  Status Put(const WriteOptions &options, std::string_view key,
             std::string_view value) override {
    return Status::kCorruption;
  }
  Status Delete(const WriteOptions &options, std::string_view key) override {
    return Status::kCorruption;
  }
};
} // namespace pedrodb

#endif // PEDRODB_DB_IMPL_H
