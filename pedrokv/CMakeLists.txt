project(pedrodb)

file(GLOB_RECURSE PEDROKV_SRCS src/*.cc)

add_library(pedrokv SHARED ${PEDROKV_SRCS} src/server.cc include/pedrokv/server.h include/pedrokv/defines.h include/pedrokv/options.h)
target_compile_features(pedrokv PRIVATE cxx_std_17)
target_include_directories(pedrokv PRIVATE include)
target_link_libraries(pedrokv PRIVATE pedrodb pedrolib pedronet)
