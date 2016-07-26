add_executable(test_switches test_switches.c)
target_link_libraries(test_switches lsdn)

add_executable(test_nl test_nl.c)
target_include_directories(test_nl PRIVATE ${MNL_INCLUDE_DIRS})
target_link_libraries(test_nl lsdn ${MNL_LIBRARIES})
