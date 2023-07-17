#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/logger/logger.h>
#include <pedrodb/segment_db.h>
#include <iostream>
#include <random>
#include "random.h"

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

using KeyValue = std::array<std::string, 2>;

Logger logger{"test"};
std::string RandomString(const std::string& prefix, size_t bytes);
std::string PaddingString(const std::string& prefix, size_t bytes, char pad);
std::vector<KeyValue> GenerateData(size_t n, size_t key_size,
                                   size_t value_size);

void TestPut(DB* db, const std::vector<KeyValue>& data);
void TestGetAll(DB* db, const std::vector<KeyValue>& data);
void TestScan(DB* db, size_t);
void TestRandomGet(DB* db, const std::vector<KeyValue>& data, size_t n);

int main() {
  pedrodb::logger::SetLevel(Logger::Level::kInfo);
  logger.SetLevel(Logger::Level::kTrace);

  Options options{};
  // options.read_cache_bytes = 0;
  // options.read_cache_bytes = 0;

  std::string path = "/tmp/test.db";
  auto db = std::make_shared<pedrodb::DBImpl>(options, path);
  auto status = db->Init();
  if (status != Status::kOk) {
    logger.Fatal("failed to open db");
  }

  size_t n_puts = 1000000;
  size_t n_delete = 0;
  size_t n_reads = 10000000;

  auto test_data = GenerateData(n_puts, 16, 50);
  TestPut(db.get(), test_data);
  TestRandomGet(db.get(), test_data, n_reads);
  TestGetAll(db.get(), test_data);
  db->Compact();
  TestScan(db.get(), 5);
  return 0;
}

std::string RandomString(const std::string& prefix, size_t bytes) {
  std::string s = prefix;
  s.reserve(bytes);
  for (size_t i = 0; i < bytes - prefix.size(); ++i) {
    char ch = i % 26 + 'a';
    s += ch;
  }
  return s;
}

std::string PaddingString(const std::string& prefix, size_t bytes, char pad) {
  std::string s = prefix;
  s.reserve(bytes);
  for (size_t i = 0; i < bytes - prefix.size(); ++i) {
    s += pad;
  }
  return s;
}

std::vector<KeyValue> GenerateData(size_t n, size_t key_size,
                                   size_t value_size) {
  std::vector<KeyValue> test_data(n);
  for (int i = 0; i < test_data.size(); ++i) {
    test_data[i][0] = PaddingString(fmt::format("key{}", i), key_size, 0);
    test_data[i][1] = RandomString(fmt::format("value{}", i), value_size);
  }
  return test_data;
}

class Reporter {
  Timestamp start_, last_;
  size_t last_count_{};
  size_t count_{};

  Logger* logger_;
  std::string topic_;

 public:
  explicit Reporter(std::string topic, Logger* log)
      : logger_(log), topic_(std::move(topic)) {
    start_ = last_ = Timestamp::Now();

    logger_->Info("Start reporting {}", topic_);
  }

  ~Reporter() {
    Duration cost = Timestamp::Now() - start_;
    logger_->Info("End report {}: count[{}], cost[{}], avg[{}/ops]", topic_,
                  count_, cost, 1000.0 * count_ / cost.Milliseconds());
  }

  void Report() {
    auto now = Timestamp::Now();
    last_count_++;
    count_++;
    if (now - last_ > Duration::Seconds(1)) {
      logger_->Info("Report {}: {}ops/s", topic_, last_count_);
      last_ = now;
      last_count_ = 0;
    }
  }
};

void TestScan(DB* db, size_t n) {
  Reporter reporter("Scan", &logger);

  for (int i = 0; i < n; ++i) {
    pedrodb::EntryIterator::Ptr iterator;
    db->GetIterator(&iterator);

    while (iterator->Valid()) {
      iterator->Next();
      reporter.Report();
    }
  }
}

void TestPut(DB* db, const std::vector<KeyValue>& data) {
  Reporter reporter("Put", &logger);
  
  for (const auto& [key, value] : data) {
    auto stat = db->Put({}, key, value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", key, value, stat);
    }

    reporter.Report();
  }
}

void TestGetAll(DB* db, const std::vector<KeyValue>& data) {
  Reporter reporter("GetAll", &logger);

  std::string get;
  for (const auto& [key, value] : data) {
    auto stat = db->Get({}, key, &get);
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", key, value, stat);
    }
    if (get != value) {
      logger.Fatal("expected {}, shows {}", value, get);
    }
    reporter.Report();
  }
}

void TestRandomGet(DB* db, const std::vector<KeyValue>& data, size_t n) {
  Reporter reporter("RandomGet", &logger);

  std::random_device mt;

  leveldb::Random random(time(nullptr));

  for (int i = 0; i < n; ++i) {
    auto x = random.Uniform(data.size());

    std::string value;
    auto stat = db->Get(ReadOptions{}, data[x][0], &value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to read {}: {}", data[x][0], stat);
    }

    if (value != data[x][1]) {
      logger.Fatal("value is not correct: key {} value{}", data[x][0], value);
    }

    reporter.Report();
  }
}
