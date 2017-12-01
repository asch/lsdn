pkg_check_modules(DAEMON libdaemon REQUIRED)

add_executable(lsctld lsctld.c)
target_link_libraries(lsctld ${DAEMON_LIBRARIES})
target_include_directories(lsctld PRIVATE ${TCL_INCLUDE_PATH} ${DAEMON_INCLUDE_DIRS})
install(
	TARGETS lsctld
	RUNTIME DESTINATION sbin
)
