
file(GLOB TEST_SUPPORT test_*.sh test_*.lsctl run run-qemu)
file(GLOB_RECURSE TEST_SUPPORT_LIB lib/common.sh)
file(GLOB_RECURSE TEST_SUPPORT_QEMU
	qemu/lsdn-guest-init
	qemu/prepare-guest-env
	qemu/serial-shell)

add_custom_target(install_test_harness ALL
	mkdir -p qemu lib
	COMMAND ln -f ${TEST_SUPPORT} .
	COMMAND ln -f ${TEST_SUPPORT_LIB} lib
	COMMAND ln -f ${TEST_SUPPORT_QEMU} qemu
	SOURCES ${TEST_SUPPORT} ${TEST_SUPPORT_LIB} ${TEST_SUPPORT_QEMU}
)

function(test_in_ns testname)
	add_executable(test_${testname} test_${testname}.c test_${testname}.sh)
	target_include_directories(test_${testname} PRIVATE ../netmodel/include)
	target_link_libraries(test_${testname} lsdn)
	add_test(
		NAME ${testname}
		COMMAND ./run run ${testname}
	)
endfunction(test_in_ns)

function(test_in_ns_lsctl testname)
	# Custom target for IDEs
	add_custom_target(test_${testname} SOURCES test_${testname}.sh test_${testname}.lsctl)
	add_test(
		NAME ${testname}
		COMMAND ./run run ${testname}
	)
endfunction(test_in_ns_lsctl)

function(test test)
	add_executable(test_${test} test_${test}.c)
	target_include_directories(test_${test} PRIVATE ${MNL_INCLUDE_DIRS} ../netmodel/include)
	target_link_libraries(test_${test} lsdn ${MNL_LIBRARIES})
	add_test(NAME ${test} COMMAND ./test_${test})
endfunction(test)

test(nettypes)
test_in_ns_lsctl(direct)
test_in_ns(vlan_lsdn)
test_in_ns_lsctl(vlan_lsctl)
test_in_ns_lsctl(vxlan_mcast)
test_in_ns(vxlan_e2e_learning)
test_in_ns(vxlan_static)
test_in_ns_lsctl(vxlan_static_large)
test_in_ns_lsctl(vxlan_static_migrate)

test_in_ns(dhcp_in_vlan)
