function(join_list values glue output)
  string (REPLACE ";" "${glue}" _tmp_str "${values}")
  set (${output} "${_tmp_str}" PARENT_SCOPE)
endfunction(join_list)

file(GLOB TEST_SUPPORT test_*.sh test_*.lsctl run run-qemu)
file(GLOB TEST_SUPPORT_PARTS parts/*.sh parts/*.lsctl)
file(GLOB_RECURSE TEST_SUPPORT_LIB lib/common.sh lib/common.tcl)
file(GLOB_RECURSE TEST_SUPPORT_QEMU
	qemu/lsdn-guest-init
	qemu/prepare-guest-env
	qemu/serial-shell)

add_custom_target(install_test_harness ALL
	mkdir -p qemu lib parts
	COMMAND ln -f ${TEST_SUPPORT} .
	COMMAND ln -f ${TEST_SUPPORT_LIB} lib
	COMMAND ln -f ${TEST_SUPPORT_PARTS} parts
	COMMAND ln -f ${TEST_SUPPORT_QEMU} qemu
	SOURCES ${TEST_SUPPORT} ${TEST_SUPPORT_LIB} ${TEST_SUPPORT_QEMU} ${TEST_SUPPORT_PARTS}
)

function(test_executable testname)
	add_executable(test_${testname} test_${testname}.c)
	target_include_directories(test_${testname} PRIVATE ../netmodel/include)
	target_link_libraries(test_${testname} lsdn)
endfunction(test_executable)

function(test_executable_vm testname)
	add_executable(test_${testname} test_${testname}.c)
	target_include_directories(test_${testname} PRIVATE ../netmodel/include ../hooks)
	target_link_libraries(test_${testname} lsdn hooks)
endfunction(test_executable_vm)

function(test_parts)
	join_list("${ARGN}" "_" testname)
	add_test(NAME ${testname} COMMAND ./run ${ARGN})
endfunction(test_parts)

function(test_simple test)
	test_executable(${test})
	add_test(NAME ${test} COMMAND ./test_${test})
endfunction(test_simple)

test_simple(nettypes)
# direct connection does not support multiple vnets, so no need to run the regular test
test_parts(direct migrate ping)
test_parts(direct migrate cleanup)

test_parts(vlan basic ping)
test_parts(vlan migrate ping)
test_parts(vlan basic cleanup)
test_parts(vlan migrate cleanup)
test_parts(vlan dhcp)

test_parts(vxlan_mcast basic ping)
test_parts(vxlan_mcast migrate ping)
test_parts(vxlan_mcast basic cleanup)
test_parts(vxlan_mcast migrate cleanup)

test_parts(vxlan_e2e basic ping)
test_parts(vxlan_e2e migrate ping)
test_parts(vxlan_e2e basic cleanup)
test_parts(vxlan_e2e migrate cleanup)

test_parts(vxlan_static basic ping)
test_parts(vxlan_static migrate ping)
test_parts(vxlan_static large ping)
test_parts(vxlan_static basic cleanup)
test_parts(vxlan_static migrate cleanup)
test_parts(vxlan_static large cleanup)
test_parts(vxlan_static dhcp)
test_parts(dhcp_in_vms)

test_executable_vm(dhcp_in_vms)
