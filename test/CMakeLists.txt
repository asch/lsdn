add_executable(test_nl test_nl.c)
target_include_directories(test_nl PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_nl lsdn ${MNL_LIBRARIES})

add_executable(test_veth test_veth.c)
target_include_directories(test_veth PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_veth lsdn ${MNL_LIBRARIES})

file(GLOB TEST_SUPPORT test_*.sh run run-qemu)
file(GLOB_RECURSE TEST_SUPPORT_LIB lib/common.sh)
file(GLOB_RECURSE TEST_SUPPORT_QEMU qemu/guest-test-runner qemu/guest-test-init)

add_custom_target(install_test_harness ALL
        mkdir -p qemu lib
        COMMAND ln -sf ${TEST_SUPPORT} .
        COMMAND ln -sf ${TEST_SUPPORT_LIB} lib
        COMMAND ln -sf ${TEST_SUPPORT_QEMU} qemu
        SOURCES ${TEST_SUPPORT} ${TEST_SUPPORT_LIB} ${TEST_SUPPORT_QEMU}
)

function(test_in_ns testname)
        add_executable(test_${testname} test_${testname}.c)
        target_link_libraries(test_${testname} lsdn)
	add_test(
		NAME ${testname}
                COMMAND ./run test ${testname}
	)
endfunction(test_in_ns)

test_in_ns(switches)
test_in_ns(linux_switch)
