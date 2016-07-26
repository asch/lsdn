add_executable(test_switches test_switches.c)
target_link_libraries(test_switches lsdn)

add_executable(test_nl test_nl.c)
target_include_directories(test_nl PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_nl lsdn ${MNL_LIBRARIES})

function(test_in_ns testname)
	add_test(
		NAME ${testname}
		COMMAND ip netns exec lsdn-test ${CMAKE_CURRENT_SOURCE_DIR}/${testname}.sh
	)
endfunction(test_in_ns)

add_custom_target(test-setup ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh setup)
add_custom_target(test-teardown ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh teardown)


add_test(NAME oktest COMMAND true)
test_in_ns(test_switches)
