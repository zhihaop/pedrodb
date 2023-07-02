cmake_minimum_required(VERSION 3.0)
project(pedrolib)

set(CMAKE_CXX_STANDARD 17)
file(GLOB_RECURSE srcs src/*.cc)

add_library(pedrolib ${srcs})
target_include_directories(pedrolib PUBLIC include)
target_link_libraries(pedrolib PRIVATE spdlog fmt)