#ifndef PRACTICE_UTILS_H
#define PRACTICE_UTILS_H

#include "proto/registry.pb.h"
#include "proto/request.pb.h"

inline static core::Request create_request(const std::string &s, int32_t id) {
  core::Request r;
  r.set_msg(s);
  r.set_id(id);
  return r;
}

inline static core::User create_user(int32_t id, const std::string &name,
                                     core::User_Gender gender) {
  core::User user;
  user.set_id(id);
  user.set_name(name);
  user.set_gender(gender);
  return user;
}

#endif // PRACTICE_UTILS_H
