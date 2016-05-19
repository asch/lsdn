add_executable(test_switches test_switches.c)
add_executable(test_nl test_nl.c)
target_link_libraries(test_switches lsdn)
target_link_libraries(test_nl lsdn)
