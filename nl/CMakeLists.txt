cmake_minimum_required (VERSION 2.8)

project (nllib)

find_package (PkgConfig)
pkg_check_modules (PC_LIBNL3 libnl-3.0)
pkg_check_modules (PC_LIBNL3_ROUTE libnl-route-3.0)

message(STATUS ${PC_LIBNL3_INCLUDE_DIRS})
message(STATUS ${PC_LIBNL3_LIBRARIES})
message(STATUS ${PC_LIBNL3_ROUTE_INCLUDE_DIRS})
message(STATUS ${PC_LIBNL3_ROUTE_LIBRARIES})

FILE (GLOB nllib_sources ${nllib_SOURCE_DIR}/*.c)

add_library (nllib ${nllib_sources})

FIND_PATH (Libnl_INCLUDE_DIR
	HINTS ${PC_LIBNL3_INCLUDE_DIRS}
	PATH_SUFFIXES include/libnl3
	NAMES /netlink/netlink.h
)

FIND_LIBRARY (Libnl_LIBRARY
	HINTS ${PC_LIBNL3_LIBRARIES}
    NAMES nl-3 nl
    PATH_SUFFIXES lib64 lib
)

FIND_LIBRARY (Libnl_route_LIBRARY
	HINTS ${PC_LIBNL3_ROUTE_LIBRARIES}
    NAMES nl-route-3 nl-route
    PATH_SUFFIXES lib64 lib
)

include_directories (${Libnl_INCLUDE_DIR})

target_link_libraries(
    nllib
    ${Libnl_LIBRARY}
    ${Libnl_route_LIBRARY}
)

# tests
add_executable(test_create_dummy test-create-dummy.c)
add_executable(test_set_dummy test-set-dummy.c)
add_executable(test_delete_link test-delete-link.c)
add_executable(test_assign_mac_addr test-assign-mac-addr.c)
add_executable(test_create_ingress_qdisc test-create-ingress-qdisc.c)
add_executable(test_redirect_ingress test-redirect-ingress.c)

target_link_libraries(test_create_dummy nllib)
target_link_libraries(test_delete_link nllib)
target_link_libraries(test_set_dummy nllib)
target_link_libraries(test_assign_mac_addr nllib)
target_link_libraries(test_create_ingress_qdisc nllib)
target_link_libraries(test_redirect_ingress nllib)
