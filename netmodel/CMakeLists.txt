find_package(PkgConfig)
pkg_check_modules(MNL libmnl)
include_directories(${MNL_INCLUDE_DIRS})

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

target_link_libraries(
	lsdn
	${MNL_LIBRARIES}
)
