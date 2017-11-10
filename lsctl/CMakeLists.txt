find_package(TCL)

add_executable(lsctl lsctl.c lsext.c)
target_link_libraries(lsctl lsdn ${TCL_LIBRARY})
target_include_directories(lsctl PRIVATE ${TCL_INCLUDE_PATH})
