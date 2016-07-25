cmake_minimum_required (VERSION 2.8)

project (mnl_test)

include_directories (${MNL_INCLUDE_DIRS})

add_executable (rtnl_link_create rtnl-link-create.c)
add_executable (rtnl_link_set rtnl-link-set.c)
add_executable (rtnl_qdisc_add_htb rtnl-qdisc-add-htb.c)

target_link_libraries (rtnl_link_create ${MNL_LIBRARIES})
target_link_libraries (rtnl_link_set ${MNL_LIBRARIES})
target_link_libraries (rtnl_qdisc_add_htb ${MNL_LIBRARIES})
