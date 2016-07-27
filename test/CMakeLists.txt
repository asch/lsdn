add_executable(test_switches test_switches.c)
target_link_libraries(test_switches lsdn)

add_executable(test_nl test_nl.c)
target_include_directories(test_nl PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_nl lsdn ${MNL_LIBRARIES})

add_executable(test_veth test_veth.c)
target_include_directories(test_veth PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_veth lsdn ${MNL_LIBRARIES})

file(GLOB TEST_SUPPORT test_*.sh harness.sh common.sh)
add_custom_target(install_test_harness ALL true
	COMMAND ln -s ${TEST_SUPPORT} .
)

function(test_in_ns testname)
	add_test(
		NAME ${testname}
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh run-test ${testname}
	)
endfunction(test_in_ns)

add_custom_target(test-setup COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh setup)
add_custom_target(test-teardown COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh teardown)

add_test(NAME oktest COMMAND true)
test_in_ns(switches)
