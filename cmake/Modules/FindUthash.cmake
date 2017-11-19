# - Try to find uthash
# Once done this will define
#
#  UTHASH_FOUND - system has uthash
#  UTHASH_INCLUDE_DIR - the uthash include directory

# Copyright (c) 2011, Dennis Schridde <dennis.schridde@stud.uni-hannover.de>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

set(UTHASH_PREFIX "" CACHE PATH "Prefix uthash was installed into")

if (UTHASH_INCLUDE_DIR)

	# in cache already
	set(UTHASH_FOUND TRUE)

else (UTHASH_INCLUDE_DIR)

	find_path(UTHASH_INCLUDE_DIR NAMES uthash.h
		PATH_SUFFIXES
			include
		HINTS
			${UTHASH_PREFIX}
			$ENV{UTHASH_PREFIX}
	)

	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(uthash DEFAULT_MSG UTHASH_INCLUDE_DIR)
	mark_as_advanced(UTHASH_INCLUDE_DIR)

endif(UTHASH_INCLUDE_DIR)
