#include "utils.h"

#include <gtest/gtest.h>

TEST(test_protobuf, test_one_of) {
  core::OneOfRequest one_of;

  auto *request = new core::Request;
  auto *repeated = new core::RepeatedRequest;
  *request = create_request("hello", 1024);

  // request and repeat can only exist one.
  one_of.set_allocated_repeated(repeated);
  // the repeated will be deleted.
  one_of.set_allocated_request(request);

  ASSERT_FALSE(one_of.has_repeated());
  ASSERT_EQ(one_of.request().msg(), request->msg());
}