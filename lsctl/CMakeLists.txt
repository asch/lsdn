add_executable(lsctl lsctl.c lsext.c lsext.h)
target_link_libraries(lsctl lsdn ${TCL_LIBRARY})
target_include_directories(lsctl PRIVATE ${TCL_INCLUDE_PATH})
install(
	TARGETS lsctl
	RUNTIME DESTINATION sbin
)
