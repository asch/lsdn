find_library(TCLSTUBS_LIBRARY
	NAMES
	tclstub
	tclstub${TCL_LIBRARY_VERSION} tclstub${TCL_TCLSH_VERSION} tclstub${TK_WISH_VERSION}
	tclstub86 tclstub8.6
	tclstub85 tclstub8.5
	tclstub84 tclstub8.4
	tclstub83 tclstub8.3
	tclstub82 tclstub8.2
	tclstub80 tclstub8.0
	PATHS ${TCLTK_POSSIBLE_LIB_PATHS}
	)

add_library(lsext SHARED lsext.h lsext.c pkgIndex.tcl)
set_target_properties(lsext PROPERTIES OUTPUT_NAME lsctl-tclext)
target_link_libraries(lsext lsdn ${TCLSTUBS_LIBRARY})
target_include_directories(lsext PRIVATE ${TCL_INCLUDE_PATH})
install(
	TARGETS lsext
	DESTINATION lib
)
install(
	FILES pkgIndex.tcl
	DESTINATION lib/lsdn)


add_executable(lsctl lsctl.c)
target_link_libraries(lsctl lsext ${TCL_LIBRARY})
target_include_directories(lsctl PRIVATE ${TCL_INCLUDE_PATH})
install(
	TARGETS lsctl
	RUNTIME DESTINATION sbin
)


