project(pedrolib VERSION 1.0.1)

file(GLOB_RECURSE srcs src/*.cc)

add_library(pedrolib SHARED ${srcs})
target_compile_features(pedrolib PRIVATE cxx_std_17)
target_include_directories(pedrolib PUBLIC include)
target_link_libraries(pedrolib PUBLIC fmt pthread)

add_executable(test_thread_pool_executor test/test_thread_pool_executor.cc)
target_include_directories(test_thread_pool_executor PUBLIC include)
target_link_libraries(test_thread_pool_executor PRIVATE pedrolib)