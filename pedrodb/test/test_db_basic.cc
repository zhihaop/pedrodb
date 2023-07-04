#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/logger/logger.h>

#include <random>
using namespace std::chrono_literals;
using pedrodb::DB;
using pedrodb::Options;
using pedrodb::ReadOptions;
using pedrodb::Status;
using pedrodb::WriteOptions;

std::string RandomString(const std::string &prefix, size_t bytes) {
  std::string s = prefix;
  s.reserve(s.size() + bytes);
  for (size_t i = 0; i < bytes; ++i) {
    char ch = i % 26 + 'a';
    s += ch;
  }
  return s;
}

int main() {
  std::shared_ptr<DB> db;
  Options options{};
  auto status = DB::Open(options, "/home/zhihaop/db/test.db", &db);
  if (status != Status::kOk) {
    PEDRODB_FATAL("failed to open db");
  }

  db->Compact();

  size_t n_puts = 1000000;
  size_t n_delete = 10000;
  size_t n_reads = 10000000;

  PEDRODB_INFO("benchmark put");
  // put
  for (size_t i = 0; i < n_puts; ++i) {
    std::string key = fmt::format("key{}", i);
    std::string value = RandomString(fmt::format("value{}", i), 2 << 10);
    auto stat = db->Put(WriteOptions{.sync = false}, key, value);
    if (stat != Status::kOk) {
      PEDRODB_FATAL("failed to write {}, {}", key, value);
    }
  }

  std::normal_distribution<double> d(5.0, 2.0);
  std::mt19937_64 g;
  PEDRODB_INFO("benchmark get");
  // get
  for (int i = 0; i < n_reads; ++i) {
    int x = d(g) + n_puts / 2;
    x = std::clamp(x, 0, (int)n_puts - 1);
    std::string key = fmt::format("key{}", x);
    std::string value;
    auto stat = db->Get(ReadOptions{}, key, &value);
    if (stat != Status::kOk) {
      PEDRODB_FATAL("failed to read {}", key);
    }

    if (value.find(fmt::format("value{}", x)) != 0) {
      PEDRODB_FATAL("value is not correct: {}", value);
    }
  }

  PEDRODB_INFO("hit ratio: {}", ((pedrodb::DBImpl *)db.get())->CacheHitRatio());

  PEDRODB_INFO("test delete");
  // delete
  for (size_t i = 0; i < n_delete; ++i) {
    std::string key = fmt::format("key{}", i);
    if (db->Delete(WriteOptions{}, key) != Status::kOk) {
      PEDRODB_FATAL("failed to delete key {}", key);
    }
  }

  // get delete
  for (size_t i = 0; i < n_delete; ++i) {
    std::string key = fmt::format("key{}", i);
    std::string value;
    if (db->Get(ReadOptions{}, key, &value) != Status::kNotFound) {
      PEDRODB_FATAL("key {} should be deleted");
    }
  }

  // get not delete
  for (size_t i = n_delete; i < n_puts; ++i) {
    std::string key = fmt::format("key{}", i);
    std::string value;
    auto stat = db->Get(ReadOptions{}, key, &value);
    if (stat != Status::kOk) {
      PEDRODB_FATAL("failed to read {}", key);
    }

    if (value.find(fmt::format("value{}", i)) != 0) {
      PEDRODB_FATAL("value is not correct: {}", value);
    }
  }

  db->Compact();
  return 0;
}