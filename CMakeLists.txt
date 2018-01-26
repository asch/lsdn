project(lsdn)
cmake_minimum_required(VERSION 2.8)
enable_testing()

option(BUILD_SHARED_LIBS "Build lsdn library as shared" ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(TCL)
find_package(PkgConfig)
find_package(Uthash REQUIRED)
pkg_check_modules(JSONC json-c REQUIRED)
pkg_check_modules(MNL libmnl REQUIRED)
pkg_check_modules(LIBVIRT libvirt)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall ${CMAKE_C_FLAGS}")

if(NOT ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    SET(OUT_TREE ON)
endif()

add_subdirectory(netmodel)
add_subdirectory(test)
add_subdirectory(lsctl)
add_subdirectory(daemon)
add_subdirectory(lsctlc)

if (LIBVIRT_FOUND)
	add_subdirectory(hooks)
	add_subdirectory(examples)
endif (LIBVIRT_FOUND)

add_custom_target(doc
	./build-doc.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
