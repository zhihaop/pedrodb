#include "utils.h"

#include <gtest/gtest.h>
#include <unordered_map>

TEST(test_protobuf, test_map) {
  core::Registry registry;

  std::unordered_map<int32_t, core::User> users;
  
  users[0] = create_user(0, "lisa", core::User_Gender_FEMALE);
  users[1] = create_user(1, "ben", core::User_Gender_MALE);
  users[2] = create_user(2, "green", core::User_Gender_MALE);

  // insert users using iterator.
  auto &mutable_users = *registry.mutable_users();
  mutable_users.insert(users.begin(), users.end());

  // scan users.
  for (auto &&[k, v] : mutable_users) {
    core::User user = users.at(k);
    ASSERT_EQ(user.name(), v.name());
  }
  
  // point access.
  ASSERT_EQ(mutable_users.at(1).name(), "ben");
  ASSERT_EQ(mutable_users[1].name(), "ben");
  
  // point modify.
  mutable_users[3] = create_user(3, "blue", core::User_Gender_MALE);
  ASSERT_EQ(mutable_users[3].name(), "blue");
}