#include <pedrokv/codec/request.h>
#include <iostream>
using namespace std;
using namespace pedrokv;

std::string Repeat(std::string_view s, size_t n) {
  std::string t;
  t.reserve(s.size() * n);
  while (n--) {
    t += s;
  }
  return t;
}

int main() {
  ArrayBuffer buffer;

  for (int i = 0; i < 10000; ++i) {
    Request request;
    request.id = i;
    request.type = pedrokv::RequestType::kPut;
    request.key = fmt::format("hello{}", i);
    request.value = fmt::format("world{}{}", i, Repeat("0", 1 << 10));
    request.Pack(&buffer);
  }
  
  ArrayBuffer target;
  target.Append(&buffer);

  for (int i = 0; i < 10000; ++i) {
    RequestView result;
    result.UnPack(&target);
    
    Request expected;
    expected.id = i;
    expected.type = pedrokv::RequestType::kPut;
    expected.key = fmt::format("hello{}", i);
    expected.value = fmt::format("world{}{}", i, Repeat("0", 1 << 10));
    
    if (expected != result) {
      cout << "error" << endl;
    }
  }
  return 0;
}