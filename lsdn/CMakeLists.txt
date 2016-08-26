add_executable(lsdn-load lsdn_load.c common.c config.c strbuf.c)

target_link_libraries(lsdn-load yaml)
target_link_libraries(lsdn-load lsdn)
