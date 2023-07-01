cmake_minimum_required(VERSION 3.0)
project(pedronet)

set(CMAKE_CXX_STANDARD 17)
file(GLOB_RECURSE srcs src/*.cc)
add_library(pedronet ${srcs})
target_include_directories(pedronet PUBLIC include)
target_link_libraries(pedronet PRIVATE spdlog fmt)

add_executable(test_tcp_server test/test_tcp_server.cc)
target_link_libraries(test_tcp_server PRIVATE pedronet)

add_executable(test_tcp_client test/test_tcp_client.cc)
target_link_libraries(test_tcp_client PRIVATE pedronet)

add_executable(test_event_loop test/test_event_loop.cc)
target_link_libraries(test_event_loop PRIVATE pedronet)