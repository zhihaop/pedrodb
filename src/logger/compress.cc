#include "pedrodb/compress.h"
#include <snappy.h>

void pedrodb::Compress(const std::string& src, std::string* dst) {
  snappy::Compress(src.data(), src.size(), dst);
}

void pedrodb::Compress(std::string_view src, std::string* dst) {
  snappy::Compress(src.data(), src.size(), dst);
}
void pedrodb::Uncompress(const std::string& src, std::string* dst) {
  snappy::Uncompress(src.data(), src.size(), dst);
}
void pedrodb::Uncompress(std::string_view src, std::string* dst) {
  snappy::Uncompress(src.data(), src.size(), dst);
}
