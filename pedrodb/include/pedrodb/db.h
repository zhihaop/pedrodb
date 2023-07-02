#ifndef PEDRODB_DB_H
#define PEDRODB_DB_H

#include "pedrodb/options.h"
#include "pedrodb/status.h"
#include <pedrolib/duration.h>
#include <pedrolib/noncopyable.h>
#include <pedrolib/timestamp.h>

namespace pedrodb {

using Timestamp = pedrolib::Timestamp;
using Duration = pedrolib::Duration;

struct DB : pedrolib::noncopyable {
  static Status Open(const Options &options, const std::string &name, DB **db);

  DB() = default;
  virtual ~DB() = default;

  virtual Status Get(const ReadOptions &options, std::string_view key,
                     std::string *value) = 0;

  virtual Status Put(const WriteOptions &options, std::string_view key,
                     std::string_view value) = 0;

  virtual Status Delete(const WriteOptions &options, std::string_view key) = 0;
};
} // namespace pedrodb

#endif // PEDRODB_DB_H
