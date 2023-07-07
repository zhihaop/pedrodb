#include <execution>
#include <iostream>
#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/logger/logger.h>
#include <pedrodb/segment_db.h>
#include <random>

using namespace std::chrono_literals;
using pedrodb::DB;
using pedrodb::Options;
using pedrodb::ReadOptions;
using pedrodb::SegmentDB;
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
  Logger logger("test");
  pedrodb::logger::SetLevel(Logger::Level::kTrace);

  Options options{};
  options.read_cache_bytes = -1;
  // options.read_cache_bytes = 0;

  std::string path = "/home/zhihaop/db/test.db";
  auto db = std::make_shared<pedrodb::DBImpl>(options, path);
  auto status = db->Init();
  if (status != Status::kOk) {
    logger.Fatal("failed to open db");
  }

  db->Compact();

  size_t n_puts = 1000000;
  size_t n_delete = 0;
  size_t n_reads = 10000000;

  logger.Info("benchmark put");

  pedrolib::ThreadPoolExecutor executor(1);
  auto mt = std::mt19937(std::random_device()());
  std::vector<int> all(n_puts);
  std::iota(all.begin(), all.end(), 0);
  std::shuffle(all.begin(), all.end(), mt);
  // put
  //  for (int i : all) {
  //    std::string key = fmt::format("key{}", i);
  //    std::string value = RandomString(fmt::format("value{}", i), 4 << 10);
  //    auto stat = db->Put(WriteOptions{.sync = false}, key, value);
  //    if (stat != Status::kOk) {
  //      logger.Fatal("failed to write {}, {}: {}", key, value, stat);
  //    }
  //  }

  //  logger.Info("benchmark get all");
  //  for (int i : all) {
  //    std::string key = fmt::format("key{}", i);
  //    std::string value;
  //    auto stat = db->Get(ReadOptions{}, key, &value);
  //    if (stat != Status::kOk) {
  //      logger.Fatal("failed to read {}: {}", key, stat);
  //    }
  //
  //    if (value.find(fmt::format("value{}", i)) != 0) {
  //      logger.Fatal("value is not correct: {}", value);
  //    }
  //  }
  logger.Info("benchmark get random");

  std::normal_distribution<double> d((double)n_puts / 2.0,
                                     (double)n_puts / 10.0);
  // get
  for (int i = 0; i < n_reads; ++i) {
    int x = std::clamp((int)d(mt), 0, (int)n_puts - 1);
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
  fmt::print("cache hit: {}\n", db->CacheHitRatio());
  return 0;

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