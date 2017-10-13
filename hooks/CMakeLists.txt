file(GLOB HOOKS "*.c" "*.h")
file(GLOB DOMAIN_GENERATOR "vm_gen")

include_directories(${LIBVIRT_INCLUDE_DIRS})

add_library(hooks ${HOOKS})

target_link_libraries(hooks ${LIBVIRT_LIBRARIES})

add_custom_target(install_hooks ALL
	mkdir qemu
	COMMAND ln -f ${DOMAIN_GENERATOR} .
)
