cmake_minimum_required(VERSION 2.8)

project (nllib)

FILE (GLOB nllib_sources ${nllib_SOURCE_DIR}/*.c)

add_library (nllib ${nllib_sources})

FIND_PATH(Libnl_INCLUDE_DIR PATH_SUFFIXES include/libnl3 NAMES /netlink/netlink.h)
FIND_LIBRARY(Libnl_LIBRARY
    NAMES nl-3 nl
    PATH_SUFFIXES lib64 lib
)

FIND_LIBRARY(Libnl_route_LIBRARY
    NAMES nl-route-3 nl-route
    PATH_SUFFIXES lib64 lib
)

include_directories(${Libnl_INCLUDE_DIR})

target_link_libraries(
    nllib
    ${Libnl_LIBRARY}
    ${Libnl_route_LIBRARY}
)

add_executable(test_create_dummy test-create-dummy.c)
add_executable(test_set_dummy test-set-dummy.c)
add_executable(test_delete_link test-delete-link.c)
add_executable(test_assign_mac_addr test-assign-mac-addr.c)

target_link_libraries(test_create_dummy nllib)
target_link_libraries(test_delete_link nllib)
target_link_libraries(test_set_dummy nllib)
target_link_libraries(test_assign_mac_addr nllib)
