#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/logger/logger.h>
#include <pedrodb/segment_db.h>
#include <random>
#include <iostream>
#include "random.h"
#include "reporter.h"

using namespace std::chrono_literals;
using pedrodb::DB;
using pedrodb::Duration;
using pedrodb::Options;
using pedrodb::ReadOptions;
using pedrodb::SegmentDB;
using pedrodb::Status;
using pedrodb::Timestamp;
using pedrodb::WriteOptions;
using pedrolib::Executor;
using pedrolib::Latch;
using pedrolib::Logger;

Logger logger{"test"};
std::string RandomString(std::string_view prefix, size_t n);
std::string PaddingString(std::string_view prefix, size_t n, char pad = '*');

struct KeyValue {
  std::string key;
  std::string value;

  static KeyValue Create(size_t index, size_t kb, size_t vb) {
    auto key = PaddingString(fmt::format("key{}", index), kb);
    auto value = RandomString(fmt::format("value{}", index), vb);
    return {key, value};
  }
};

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

  std::string path = "/tmp/test.db";
  auto db = std::make_shared<pedrodb::DBImpl>(options, path);
  auto status = db->Init();
  if (status != Status::kOk) {
    logger.Fatal("failed to open db");
  }
  
  std::cin.get();

  size_t n_puts = 1000000;
  size_t n_reads = 1000000;

  auto data = GenerateData(n_puts, 16, 100);
  TestPut(db.get(), data);
  TestRandomGet(db.get(), data, n_reads);
  TestGetAll(db.get(), data);
  db->Compact();
  TestScan(db.get(), 5);
  return 0;
}

std::string RandomString(std::string_view prefix, size_t n) {
  std::string s{prefix};
  s.reserve(n);

  std::uniform_int_distribution<char> dist(0, 127);
  thread_local std::mt19937_64 dev(std::time(nullptr));
  for (size_t i = 0; i < n - prefix.size(); ++i) {
    s += dist(dev);
  }
  return s;
}

std::string PaddingString(std::string_view prefix, size_t n, char pad) {
  std::string s{prefix};
  s.reserve(n);
  for (size_t i = 0; i < n - prefix.size(); ++i) {
    s += pad;
  }
  return s;
}

std::vector<KeyValue> GenerateData(size_t n, size_t key_size,
                                   size_t value_size) {
  std::vector<KeyValue> data(n);
  pedrolib::ThreadPoolExecutor executor(16);
  Latch latch(n);
  for (size_t i = 0; i < n; ++i) {
    executor.Schedule([=, &data, &latch] {
      data[i] = KeyValue::Create(i, key_size, value_size);
      latch.CountDown();
    });
  }
  latch.Await();
  return data;
}

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

  for (const auto& kv : data) {
    auto stat = db->Put({}, kv.key, kv.value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", kv.key, kv.value, stat);
    }

    reporter.Report();
  }
  db->Flush();
}

void TestGetAll(DB* db, const std::vector<KeyValue>& data) {
  Reporter reporter("GetAll", &logger);

  std::string get;
  ReadOptions options;
  for (const auto& kv : data) {
    auto stat = db->Get(options, kv.key, &get);
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", kv.key, kv.value, stat);
    }
    if (get != kv.value) {
      logger.Fatal("expected {}, shows {}", kv.value, get);
    }
    reporter.Report();
  }
}

void TestRandomGet(DB* db, const std::vector<KeyValue>& data, size_t n) {
  Reporter reporter("RandomGet", &logger);

  leveldb::Random random(time(nullptr));

  for (int i = 0; i < n; ++i) {
    auto x = random.Uniform((int)data.size());

    std::string value;
    auto stat = db->Get(ReadOptions{}, data[x].key, &value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to read {}: {}", data[x].key, stat);
    }

    if (value != data[x].value) {
      logger.Fatal("value is not correct: key {} value{}", data[x].value,
                   value);
    }

    reporter.Report();
  }
}
