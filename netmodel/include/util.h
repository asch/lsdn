#ifndef _LSDN_UTIL_H
#define _LSDN_UTIL_H

#define UNUSED(x) ((void)(x))

#define	DEBUG_PRINTF(fmt, ...) do { \
	if (DEBUG) fprintf(stderr, "*** DEBUG OUTPUT ***\t" fmt " at %s:%d\n", \
		##__VA_ARGS__, __FILE__, __LINE__); \
} while (0);

#define	DEBUG_MSG(msg)			DEBUG_PRINTF("%s", msg);
#define	DEBUG_EXPR(formatter, expr)	DEBUG_PRINTF(#expr " = " formatter, (expr));

#define	NOT_IMPLEMENTED	do {\
	DEBUG_PRINTF("%s: not implemented", __func__); \
	exit(EXIT_FAILURE); \
} while (0);

#endif
