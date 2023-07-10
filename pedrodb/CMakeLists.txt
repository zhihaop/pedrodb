project(pedrodb)

file(GLOB_RECURSE PEDRODB_SRCS src/*.cc)

add_library(pedrodb SHARED ${PEDRODB_SRCS})
target_compile_features(pedrodb PRIVATE cxx_std_17)
target_include_directories(pedrodb PUBLIC include)
target_link_libraries(pedrodb PRIVATE pedrolib)

add_executable(test_db_basic test/test_db_basic.cc)
target_include_directories(test_db_basic PUBLIC include)
target_link_libraries(test_db_basic PRIVATE pedrodb pedrolib)


