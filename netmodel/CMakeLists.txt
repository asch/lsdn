set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules;${CMAKE_MODULE_PATH}")

find_package (MNL REQUIRED)

add_library(lsdn 
   network.c 
   port.c
   node.c
   static_switch.c
   netdev.c
   internal.h
   nettypes.c
   rule.c
   tc.c
   list.c
   errors.h
   lsdn.h
   util.h
   nl.h
   nl.c
)

include_directories (${MNL_INCLUDE_DIRS})

target_link_libraries(
	lsdn
	${MNL_LIBRARIES}
)
