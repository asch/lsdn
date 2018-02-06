find_package(TCL 8.6.0)
pkg_check_modules(DAEMON libdaemon REQUIRED)

add_executable(lsctld lsctld.c)
target_link_libraries(lsctld lsdn lsext ${DAEMON_LIBRARIES} ${TCL_LIBRARY})
target_include_directories(lsctld PRIVATE ${TCL_INCLUDE_PATH} ${DAEMON_INCLUDE_DIRS})
install(
	TARGETS lsctld
	RUNTIME DESTINATION bin
)
