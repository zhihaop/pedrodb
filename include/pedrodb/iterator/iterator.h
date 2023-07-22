#ifndef PEDRODB_ITERATOR_ITERATOR_H
#define PEDRODB_ITERATOR_ITERATOR_H

#include <memory>
#include "pedrodb/format/record_format.h"

namespace pedrodb {
template <typename T>
struct Iterator {
  using Ptr = std::unique_ptr<Iterator<T>>;
  
  Iterator() = default;
  virtual ~Iterator() = default;
  virtual bool Valid() = 0;
  virtual T Next() = 0;
  virtual void Close() = 0;
};
}  // namespace pedrodb

#endif  //PEDRODB_ITERATOR_ITERATOR_H
