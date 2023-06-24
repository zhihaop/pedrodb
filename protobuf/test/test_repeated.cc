#include "utils.h"

#include <gtest/gtest.h>

TEST(test_protocol, test_repeated) {
  core::RepeatedRequest repeated;

  // add repeated request.
  auto &reqs = *repeated.mutable_reqs();
  reqs.Reserve(100);
  for (int i = 0; i < 100; ++i) {
    reqs.Add(create_request("hello", i));
  }
  ASSERT_EQ(reqs.size(), 100);
  
  // access repeated data.
  const core::Request *const *data = reqs.data();
  for (int i = 0; i < 100; ++i) {
    ASSERT_EQ(data[i]->id(), i);
    ASSERT_EQ(reqs[i].id(), i);
    ASSERT_EQ(reqs.at(i).id(), i);
  }

  // find the request with id 1.
  auto it = std::find_if(reqs.begin(), reqs.end(),
                         [&](const core::Request &r) { return r.id() == 1; });
  ASSERT_TRUE(it != reqs.end());
  ASSERT_EQ(it->id(), 1);

  // erase the request with id 1.
  reqs.erase(it);
  ASSERT_EQ(reqs.size(), 99);

  // remove the last element.
  core::Request *req = reqs.ReleaseLast();
  ASSERT_EQ(reqs.size(), 98);
  delete req;

  reqs.RemoveLast();
  ASSERT_EQ(reqs.size(), 97);

  // clear the repeated data.
  reqs.Clear();
  ASSERT_TRUE(reqs.empty());
}