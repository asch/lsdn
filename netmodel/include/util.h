#ifndef _LSDN_UTIL_H
#define _LSDN_UTIL_H

#define LSDN_UNUSED(x) ((void)(x))

#define	LSDN_NOT_IMPLEMENTED	do {\
	DEBUG_PRINTF("%s: not implemented", __func__); \
	exit(EXIT_FAILURE); \
} while (0);

#endif
