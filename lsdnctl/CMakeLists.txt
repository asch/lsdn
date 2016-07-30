add_executable(lsdnctl-load lsdnctl-load.c common.c config.c)

target_link_libraries(lsdnctl-load yaml)
target_link_libraries(lsdnctl-load lsdn)
