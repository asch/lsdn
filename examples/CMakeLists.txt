file(GLOB EXAMPLES *.c *.h *.sh)

add_custom_target(install_examples ALL
    COMMAND ln -f ${EXAMPLES} .
    SOURCES ${EXAMPLES}
)

function(executable_example example)
	add_executable(${example} ${example}.c)
	target_include_directories(${example} PRIVATE ../netmodel/include ../hooks)
	target_link_libraries(${example} lsdn hooks)
endfunction(executable_example)

executable_example(dhcp_in_vms)
