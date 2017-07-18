add_executable(lsctl lsctl.c lsext.c)

target_link_libraries(lsctl lsdn tcl)
