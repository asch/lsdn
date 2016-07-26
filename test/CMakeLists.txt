add_executable(test_switches test_switches.c)
target_link_libraries(test_switches lsdn)

add_executable(test_nl test_nl.c)
target_include_directories(test_nl PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_nl lsdn ${MNL_LIBRARIES})

add_test(NAME oktest COMMAND true)
add_test(switches ip netns exec lsdn-test ${CMAKE_CURRENT_SOURCE_DIR}/test_switches.sh)

add_custom_target(test-setup ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh setup)
add_custom_target(test-teardown ${CMAKE_CURRENT_SOURCE_DIR}/harness.sh teardown)
