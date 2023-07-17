#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/logger/logger.h>
#include <pedrodb/segment_db.h>
#include <iostream>
#include <random>

using namespace std::chrono_literals;
using pedrodb::DB;
using pedrodb::Duration;
using pedrodb::Options;
using pedrodb::ReadOptions;
using pedrodb::SegmentDB;
using pedrodb::Status;
using pedrodb::Timestamp;
using pedrodb::WriteOptions;
using pedrolib::Logger;

std::string RandomString(const std::string& prefix, size_t bytes) {

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
  pedrodb::logger::SetLevel(Logger::Level::kInfo);

  Options options{};
  // options.read_cache_bytes = 0;
  // options.read_cache_bytes = 0;

  logger.SetLevel(Logger::Level::kTrace);

  std::string path = "/tmp/test.db";
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

  size_t count = 0;
  Timestamp now = Timestamp::Now();

  // put
  for (int i : all) {
    std::string key = fmt::format("key{}", i);
    std::string value = RandomString(fmt::format("value{}", i), 50);
    auto stat = db->Put(WriteOptions{.sync = false}, key, value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", key, value, stat);
    }
    count++;
    if (Timestamp::Now() - now >= Duration::Seconds(1)) {
      logger.Info("put {}/s", count);
      count = 0;
      now = Timestamp::Now();
    }
  }

  std::shuffle(all.begin(), all.end(), mt);
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
    
    count++;
    if (Timestamp::Now() - now >= Duration::Seconds(1)) {
      logger.Info("get {}/s", count);
      count = 0;
      now = Timestamp::Now();
    }
  }
  logger.Info("benchmark get random");

  std::normal_distribution<double> d((double)n_puts / 2.0,
                                     (double)n_puts / 120.0);
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
      logger.Fatal("value is not correct: key {} value{}", key, value);
    }
    
    count++;
    if (Timestamp::Now() - now >= Duration::Seconds(1)) {
      logger.Info("get random {}/s", count);
      count = 0;
      now = Timestamp::Now();
    }
  }
  logger.Info("cache hit: {}\n", db->CacheHitRatio());
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