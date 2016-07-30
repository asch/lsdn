#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#ifndef	DEBUG
#define	DEBUG	1
#endif

#define	DEBUG_PRINTF(fmt, ...) do { \
	if (DEBUG) fprintf(stderr, "*** DEBUG *** " fmt " at %s:%d\n", \
		__VA_ARGS__, __FILE__, __LINE__); \
} while (0);

#define	DEBUG_MSG(msg)			DEBUG_PRINTF("%s", msg);
#define	DEBUG_EXPR(formatter, expr)	DEBUG_PRINTF(#expr " = " formatter, (expr));

#define NOT_IMPLEMENTED	do {\
	DEBUG_PRINTF("%s: not implemented", __func__);\
	exit(EXIT_FAILURE);\
} while (0);


void *malloc_safe(size_t size);
void *realloc_safe(void *ptr, size_t size);

#endif
