cmake_minimum_required(VERSION 3.1)
project(pedrodb)

if (NOT TARGET pedrolib)
    add_subdirectory(deps/pedrolib)
endif ()
file(GLOB_RECURSE PEDRODB_SRCS src/*.cc)

add_library(pedrodb SHARED ${PEDRODB_SRCS})
target_compile_features(pedrodb PRIVATE cxx_std_17)
target_include_directories(pedrodb PUBLIC include deps/hat-trie/include)
target_link_libraries(pedrodb PRIVATE pedrolib snappy)

add_executable(pedrodb_test_basic test/test_basic.cc)
target_compile_features(pedrodb_test_basic PRIVATE cxx_std_17)
target_include_directories(pedrodb_test_basic PUBLIC include)
target_link_libraries(pedrodb_test_basic PRIVATE pedrodb pedrolib)

add_executable(pedrodb_test_disk_speed test/test_disk_speed.cc)
target_compile_features(pedrodb_test_disk_speed PRIVATE cxx_std_17)
target_include_directories(pedrodb_test_disk_speed PUBLIC include)
target_link_libraries(pedrodb_test_disk_speed PRIVATE pedrodb pedrolib)