project(lsdn)
cmake_minimum_required(VERSION 2.8)

enable_testing()

find_package(PkgConfig)
pkg_check_modules(MNL libmnl REQUIRED)
pkg_check_modules(LIBVIRT libvirt)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall ${CMAKE_C_FLAGS}")

add_subdirectory(netmodel)
add_subdirectory(test)
add_subdirectory(lsctl)

if (LIBVIRT_FOUND)
	add_subdirectory(hooks)
	add_subdirectory(examples)
endif (LIBVIRT_FOUND)
