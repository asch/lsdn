project(lsdn)
cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_check_modules(MNL libmnl)

set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_subdirectory(netmodel)
add_subdirectory(test)
