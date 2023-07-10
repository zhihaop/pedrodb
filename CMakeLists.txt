cmake_minimum_required(VERSION 3.1)

project(pedros)
find_package(fmt REQUIRED)

add_subdirectory(pedrodb)
add_subdirectory(pedrokv)
add_subdirectory(pedrolib)
add_subdirectory(pedronet)