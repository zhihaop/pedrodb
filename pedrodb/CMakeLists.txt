cmake_minimum_required(VERSION 3.0)
project(pedrodb)

set(CMAKE_CXX_STANDARD 17)
file(GLOB_RECURSE srcs src/*.cc)

add_subdirectory(../pedronet pedronet)
add_executable(pedrodb ${srcs})
target_include_directories(pedrodb PUBLIC include ../pedronet/include)
target_link_libraries(pedrodb PRIVATE pedronet)