project(pedronet)

file(GLOB_RECURSE PEDRONET_SRCS src/*.cc)

add_library(pedronet SHARED ${PEDRONET_SRCS})
target_compile_features(pedronet PRIVATE cxx_std_17)
target_include_directories(pedronet PUBLIC include)
target_link_libraries(pedronet PRIVATE pedrolib)

add_executable(test_tcp_server test/test_tcp_server.cc)
target_compile_features(test_tcp_server PRIVATE cxx_std_17)
target_link_libraries(test_tcp_server PRIVATE pedronet pedrolib)

add_executable(test_tcp_client test/test_tcp_client.cc)
target_compile_features(test_tcp_client PRIVATE cxx_std_17)
target_link_libraries(test_tcp_client PRIVATE pedronet pedrolib)

add_executable(test_event_loop test/test_event_loop.cc)
target_compile_features(test_event_loop PRIVATE cxx_std_17)
target_link_libraries(test_event_loop PRIVATE pedronet pedrolib)