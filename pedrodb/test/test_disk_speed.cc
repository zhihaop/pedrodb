#include <pedrolib/file/file.h>
#include <pedrolib/timestamp.h>
#include <iostream>
#include <random>
#include <string_view>
using pedrolib::File;
using pedrolib::Timestamp;

const size_t kBlockSize = 256 << 10;
const size_t kSyncInterval = 64 << 20;
const size_t kGiB = 1 << 30;

std::mt19937_64 gen;

void RandomString(std::string* data, size_t n) {
  data->resize(n);
  for (char& ch : *data) {
    ch = gen();
  }
}

int main() {
  File::OpenOption option;
  option.create = 0777;
  option.mode = File::OpenMode::kReadWrite;

  auto file = File::Open("/tmp/test.bin", option);
  
  std::string s;
  RandomString(&s, kBlockSize);
  Timestamp st = Timestamp::Now();
  for (size_t i = 0; i < 5 * kGiB; i += kBlockSize) {
    ssize_t n = file.Write(s.data(), s.size());
    if (n != s.size()) {
      std::cerr << "error" << std::endl;
      return -1;
    }
    if (i % kSyncInterval == 0) {
      file.Sync();
    }
  }
  file.Sync();
  Timestamp et = Timestamp::Now();

  fmt::print("{}", (et - st));
  return 0;
}