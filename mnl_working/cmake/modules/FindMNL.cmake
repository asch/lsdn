# Find 'libmnl' library
# 
#  MNL_INCLUDE_DIRS
#  MNL_LIBRARIES
#  MNL_FOUND

cmake_minimum_required (VERSION 2.8)

find_package (PkgConfig)
pkg_check_modules (PC_LIBMNL3 libmnl)

find_path (
	MNL_INCLUDE_DIRS
	NAMES libmnl/libmnl.h
	HINTS ${PC_LIBMNL3_INCLUDEDIR}
	PATH_SUFFIXES libmnl
)

find_library (
	MNL_LIBRARIES
	NAMES mnl
	HINTS ${PC_LIBMNL3_LIBRARIES}
	PATH_SUFFIXES lib64 lib
)

if (MNL_INCLUDE_DIRS AND MNL_LIBRARIES)
	set (MNL_FOUND true)
else (MNL_INCLUDE_DIRS AND MNL_LIBRARIES)
	set (MNL_FOUND false)
endif (MNL_INCLUDE_DIRS AND MNL_LIBRARIES)
