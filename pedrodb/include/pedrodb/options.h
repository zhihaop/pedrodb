#ifndef PEDRODB_OPTIONS_H
#define PEDRODB_OPTIONS_H

#include <string>

namespace pedrodb {
struct Options {
  int16_t max_open_files = 32;
};
struct ReadOptions {};
struct WriteOptions {
  bool sync = false;
};
} // namespace pedrodb

#endif // PEDRODB_OPTIONS_H
