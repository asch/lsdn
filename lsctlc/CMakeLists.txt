add_executable(lsctlc main.c)
target_link_libraries(lsctlc ${TCL_LIBRARY})
target_include_directories(lsctlc PRIVATE ${TCL_INCLUDE_PATH})
install(
	TARGETS lsctlc
	RUNTIME DESTINATION bin
)
