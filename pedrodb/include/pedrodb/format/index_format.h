#ifndef PEDROKV_FORMAT_INDEX_FORMAT_H
#define PEDROKV_FORMAT_INDEX_FORMAT_H

namespace pedrodb {
struct IndexEntry {
  std::string key;
  uint32_t offset;
  uint32_t len;
};
}

#endif  //PEDROKV_FORMAT_INDEX_FORMAT_H
