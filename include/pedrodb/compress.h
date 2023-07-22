#ifndef PEDRODB_COMPRESS_H
#define PEDRODB_COMPRESS_H

#include <string>
#include <string_view>

namespace pedrodb {

void Compress(const std::string& src, std::string* dst);
void Compress(std::string_view src, std::string* dst);
void Uncompress(const std::string& src, std::string* dst);
void Uncompress(std::string_view src, std::string* dst);
}  // namespace pedrodb
#endif  //PEDRODB_COMPRESS_H
