add_executable(lsdnctl-load lsdnctl-load.c parser.c strbuf.c common.c)

target_link_libraries(lsdnctl-load yaml)
target_link_libraries(lsdnctl-load lsdn)
