cmake_minimum_required(VERSION 3.0)
project(pedrodb)

set(CMAKE_CXX_STANDARD 17)
file(GLOB_RECURSE srcs src/*.cc)

#add_subdirectory(../pedrolib pedrolib)
add_subdirectory(../pedronet pedronet)
add_library(pedrodb ${srcs} include/pedrodb/file/readable_file.h)
target_include_directories(pedrodb PUBLIC include ../pedrolib/include)
target_link_libraries(pedrodb PRIVATE pedrolib)

add_executable(test_db_basic test/test_db_basic.cc)
target_include_directories(test_db_basic PUBLIC include ../pedrolib/include)
target_link_libraries(test_db_basic PRIVATE pedrodb pedrolib)


