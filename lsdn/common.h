#ifndef COMMON_H
#define COMMON_H

#include "../netmodel/include/util.h"
#include <stddef.h>

#ifndef	DEBUG
#define	DEBUG	1
#endif

void *malloc_safe(size_t size);
void *realloc_safe(void *ptr, size_t size);
char *strdup_safe(const char* str);
void abort_msg(const char* msg, ...);

#endif
