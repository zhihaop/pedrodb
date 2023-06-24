#include "utils.h"

#include <gtest/gtest.h>

TEST(test_protobuf, test_create) {
  core::Request request = create_request("test", 1000);

  ASSERT_EQ(request.id(), 1000);
  ASSERT_EQ(request.msg(), "test");
}

TEST(test_protobuf, test_copy) {
  core::Request request = create_request("hello", 1024);

  core::Request copy;
  copy = request;
  ASSERT_EQ(copy.msg(), request.msg());

  copy.clear_msg();
  copy.CopyFrom(request);
  ASSERT_EQ(copy.msg(), request.msg());
}

TEST(test_protobuf, test_swap) {
  core::Request r1 = create_request("hello", 1024);
  core::Request r2 = create_request("world", 2048);
  
  std::swap(r1, r2);
  ASSERT_EQ(r1.msg(), "world");
  
  r1.Swap(&r2);
  ASSERT_EQ(r1.msg(), "hello");
}

TEST(test_protobuf, test_move) {
  core::Request r1 = create_request("hello", 1024);
  core::Request r2 = std::move(r1);
  
  // the r1 request will be empty.
  ASSERT_FALSE(r1.has_msg());
  ASSERT_EQ(r2.msg(), "hello");
}

TEST(test_protobuf, test_merge) {
  core::RepeatedRequest r1;
  r1.set_id(1);
  r1.add_reqs()->CopyFrom(create_request("r0", 0));
  r1.add_reqs()->CopyFrom(create_request("r1", 1));

  core::RepeatedRequest r2;
  r2.set_id(2);
  r2.add_reqs()->CopyFrom(create_request("r3", 3));
  r2.add_reqs()->CopyFrom(create_request("r4", 4));
  
  // merge two repeated request.
  r1.MergeFrom(r2);
  ASSERT_EQ(r1.reqs_size(), 4);
  
  // the non repeated field will be replaced.
  ASSERT_EQ(r1.id(), 2);
  
  // the repeated field will be appended.
  ASSERT_EQ(r1.reqs(1).msg(), "r1");
  ASSERT_EQ(r1.reqs(3).msg(), "r4");
}

TEST(test_protobuf, test_serialize) {
  core::Request request = create_request("hello", 1024);
  
  // serialize request to std::string.
  std::string data = request.SerializeAsString();
  
  // cannot deserialize to core::RepeatedRequest.
  core::RepeatedRequest repeated;
  ASSERT_FALSE(repeated.ParseFromString(data));
  
  core::Request other;
  ASSERT_TRUE(other.ParseFromString(data));
  ASSERT_EQ(other.msg(), request.msg());

  // the empty.id is empty. but in the peer, empty.id is not empty.
  core::Request empty;
  ASSERT_FALSE(empty.has_id());
  
  data = request.SerializeAsString();
  empty.ParseFromString(data);

  ASSERT_TRUE(empty.has_id());
}