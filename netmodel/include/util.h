/** \file
 * Utility macros */
#pragma once

/** Mark argument as explicitly unused.
 * Useful in callbacks methods that take a void argument, to indicate that the
 * argument is not being used and prevent compiler warnings. */
#define LSDN_UNUSED(x) ((void)(x))

/** Throw a "not implemented" error.
 * Used in body of a function that is part of the API but has not been
 * implemented yet.  There should be no usages of this macro when the project is
 * done. */
#define	LSDN_NOT_IMPLEMENTED	do {\
	DEBUG_PRINTF("%s: not implemented", __func__); \
	exit(EXIT_FAILURE); \
} while (0);
