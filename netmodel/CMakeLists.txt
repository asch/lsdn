file(GLOB lsdn_SRC "*.c")
file(GLOB lsdn_PUBLIC "include/*.h")
file(GLOB lsdn_PRIVATE "private/*.h")

include_directories(${MNL_INCLUDE_DIRS})

add_library(lsdn 
   ${lsdn_SRC}
   ${lsdn_PUBLIC}
   ${lsdn_PRIVATE}
)

target_link_libraries(
	lsdn
	${MNL_LIBRARIES}
)
