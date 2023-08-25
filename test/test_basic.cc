#include <pedrodb/db.h>
#include <pedrodb/db_impl.h>
#include <pedrodb/debug/data.h>
#include <pedrodb/debug/reporter.h>
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
using pedrodb::debug::Generator;
using pedrodb::debug::KeyValue;
using pedrodb::debug::KeyValueOptions;
using pedrodb::debug::Reporter;
using pedrolib::Executor;
using pedrolib::Latch;
using pedrolib::Logger;

Logger logger{"test"};

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

  KeyValueOptions data_options;
  data_options.key_size = 16;
  data_options.value_size = 100;
  data_options.random_value = true;
  data_options.lazy_value = false;

  db->Compact();

  auto data = Generator(n_puts, data_options);
  TestPut(db.get(), data);
  TestRandomGet(db.get(), data, n_reads);
  TestGetAll(db.get(), data);
  TestScan(db.get(), 5);
  return 0;
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
  WriteOptions options;
  for (const auto& kv : data) {
    auto stat = db->Put(options, kv.key(), kv.value());
    if (stat != Status::kOk) {
      logger.Fatal("failed to write {}, {}: {}", kv.key(), kv.value(), stat);
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
    auto stat = db->Get(options, kv.key(), &get);
    if (stat != Status::kOk) {
      logger.Fatal("failed to Get {}, {}: {}", kv.key(), kv.value(), stat);
    }
    if (get != kv.value()) {
      logger.Fatal("expected {}, shows {}", kv.value(), get);
    }
    reporter.Report();
  }
}

void TestRandomGet(DB* db, const std::vector<KeyValue>& data, size_t n) {
  Reporter reporter("RandomGet", &logger);

  leveldb::Random random(time(nullptr));
  ReadOptions options;
  for (int i = 0; i < n; ++i) {
    auto x = random.Uniform((int)data.size());

    std::string value;
    auto stat = db->Get(options, data[x].key(), &value);
    if (stat != Status::kOk) {
      logger.Fatal("failed to read {}: {}", data[x].key(), stat);
    }

    if (value != data[x].value()) {
      logger.Fatal("value is not correct: key {} value{}", data[x].value(),
                   value);
    }

    reporter.Report();
  }
}
