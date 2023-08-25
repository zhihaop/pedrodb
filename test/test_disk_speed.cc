#include <pedrolib/file/file.h>
#include <pedrolib/timestamp.h>
#include <sys/mman.h>
#include <iostream>
#include <random>

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

void TestWrite(File& file) {
  file.Reserve(5 * kGiB);
  file.Seek(0, File::Whence::kSeekSet);

  std::string s;
  RandomString(&s, kBlockSize);
  Timestamp st = Timestamp::Now();
  for (size_t i = 0; i < 5 * kGiB; i += kBlockSize) {
    ssize_t n = file.Write(s.data(), s.size());
    if (n != s.size()) {
      std::cerr << "error" << std::endl;
      return;
    }
    if (i % kSyncInterval == 0) {
      file.Sync();
    }
  }
  file.Sync();
  Timestamp et = Timestamp::Now();

  int ms = (et - st).Milliseconds();
  fmt::print("{:.2} GiB/s\n", 5.0 / ms * 1000);
}

void TestMmap(File& file) {
  file.Reserve(5 * kGiB);
  file.Seek(0, File::Whence::kSeekSet);
  
  char* buf = (char*) mmap(nullptr, file.GetSize(), PROT_READ | PROT_WRITE, MAP_SHARED, file.Descriptor(), 0);

  std::string s;
  RandomString(&s, kBlockSize);
  Timestamp st = Timestamp::Now();
  size_t offset = 0;
  for (size_t i = 0; i < 5 * kGiB; i += kBlockSize) {
    memcpy(buf + offset, s.data(), s.size());
    offset += s.size();
    if (i % kSyncInterval == 0) {
      msync(buf, 5 * kGiB, MS_SYNC);
    }
  }
  msync(buf, 5 * kGiB, MS_SYNC);
  Timestamp et = Timestamp::Now();

  int ms = (et - st).Milliseconds();
  fmt::print("{:.2} GiB/s\n", 5.0 / ms * 1000);
}

int main() {
  File::OpenOption option;
  option.create = 0777;
  option.mode = File::OpenMode::kReadWrite;

  auto file = File::Open("/tmp/test.bin", option);

  TestWrite(file);
  TestMmap(file);
  return 0;
}