file(GLOB lsdn_SRC "*.c")
file(GLOB lsdn_PUBLIC "include/*.h")
file(GLOB lsdn_PRIVATE "private/*.h")

include_directories(${MNL_INCLUDE_DIRS} ${KERNEL_HEADERS} ${UTHASH_INCLUDE_DIR} ${JSONC_INCLUDE_DIRS})
configure_file(lsdn.pc.in lsdn.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lsdn.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

add_library(lsdn
   ${lsdn_SRC}
   ${lsdn_PUBLIC}
   ${lsdn_PRIVATE}
)
target_link_libraries(
	lsdn
	${MNL_LIBRARIES} pthread
	${JSONC_LIBRARIES}
)
set_target_properties(lsdn PROPERTIES PUBLIC_HEADER
	"include/errors.h;include/lsdn.h;include/nettypes.h;include/util.h;include/dump.h")

install(
	TARGETS lsdn
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lsdn
)

set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags ${lsdn_SRC} ${lsdn_PUBLIC} ${lsdn_PRIVATE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
