#include <execution>
#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/logger/logger.h>
#include <pedrodb/segment_db.h>
#include <random>
using namespace std::chrono_literals;
using pedrodb::DB;
using pedrodb::Options;
using pedrodb::ReadOptions;
using pedrodb::Status;
using pedrodb::WriteOptions;
using pedrolib::Logger;

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

  options.executor = std::make_shared<pedrodb::DefaultExecutor>(1);
  // options.read_cache_bytes = 0;

  Logger logger("test");

  pedrodb::logger::SetLevel(Logger::Level::kError);

  auto status = pedrodb::DB::Open(options, "/home/zhihaop/db/test.db", &db);
  if (status != Status::kOk) {
    logger.Fatal("failed to open db");
  }

  db->Compact();

  size_t n_puts = 1000000;
  size_t n_delete = 0;
  size_t n_reads = 10000000;

  logger.Info("benchmark put");

  pedrolib::ThreadPoolExecutor executor(1);

  // put
  for (int i = 0; i < n_puts; ++i) {
    std::string key = fmt::format("key{}", i);
    std::string value = RandomString(fmt::format("value{}", i), 4 << 10);
    auto stat = db->Put(WriteOptions{.sync = false}, key, value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", key, value, stat);
    }
  }

  std::normal_distribution<double> d(5.0, 2.0);
  std::mt19937_64 g;
  std::vector<int> all(n_puts);
  std::iota(all.begin(), all.end(), 0);
  std::shuffle(all.begin(), all.end(), std::mt19937(std::random_device()()));

  logger.Info("benchmark get all");
  for (int i : all) {
    std::string key = fmt::format("key{}", i);
    std::string value;
    auto stat = db->Get(ReadOptions{}, key, &value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to read {}: {}", key, stat);
    }

    if (value.find(fmt::format("value{}", i)) != 0) {
      logger.Fatal("value is not correct: {}", value);
    }
  }
  logger.Info("hit ratio: {}", ((pedrodb::DBImpl *)db.get())->CacheHitRatio());
  logger.Info("benchmark get random");
  // get
  for (int i = 0; i < n_reads; ++i) {
    int x = d(g) + n_puts / 2;
    x = std::clamp(x, 0, (int)n_puts - 1);
    std::string key = fmt::format("key{}", x);
    std::string value;
    auto stat = db->Get(ReadOptions{}, key, &value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to read {}: {}", key, stat);
    }

    if (value.find(fmt::format("value{}", x)) != 0) {
      logger.Fatal("value is not correct: {}", value);
    }
  }

  logger.Info("hit ratio: {}", ((pedrodb::DBImpl *)db.get())->CacheHitRatio());

  logger.Info("test delete");
  // delete
  for (size_t i = 0; i < n_delete; ++i) {
    std::string key = fmt::format("key{}", i);
    if (db->Delete(WriteOptions{}, key) != Status::kOk) {
      logger.Fatal("failed to delete key {}", key);
    }
  }

  // get delete
  for (size_t i = 0; i < n_delete; ++i) {
    std::string key = fmt::format("key{}", i);
    std::string value;
    if (db->Get(ReadOptions{}, key, &value) != Status::kNotFound) {
      logger.Fatal("key {} should be deleted");
    }
  }

  // get not delete
  for (size_t i = n_delete; i < n_puts; ++i) {
    std::string key = fmt::format("key{}", i);
    std::string value;
    auto stat = db->Get(ReadOptions{}, key, &value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to read {}", key);
    }

    if (value.find(fmt::format("value{}", i)) != 0) {
      logger.Fatal("value is not correct: {}", value);
    }
  }

  return 0;
}