#include "proto/registry.pb.h"
#include "utils.h"

#include <gtest/gtest.h>

TEST(test_protobuf, test_any) {
  core::AnyRequest request;
  ASSERT_FALSE(request.has_value());

  // pack core::User as Any.
  core::User user = create_user(1, "test", core::User_Gender_MALE);
  request.mutable_value()->PackFrom(user);
  ASSERT_EQ(request.value().Is<core::User>(), true);

  // serialize and deserialize.
  std::string data = request.SerializeAsString();
  core::AnyRequest another;
  another.ParseFromString(data);

  // Unpack core::User from Any.
  core::User someone;
  ASSERT_EQ(another.value().Is<core::User>(), true);
  ASSERT_EQ(another.mutable_value()->UnpackTo(&someone), true);
  ASSERT_EQ(someone.name(), user.name());
}