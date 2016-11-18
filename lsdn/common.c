#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void abort_msg(const char* msg, ...){
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	abort();
}

char *strdup_safe(const char* str)
{
	char* ret = strdup(str);
	if(!ret)
		abort_msg("Failed to allocate memory for strdup");
	return ret;
}

void *realloc_safe(void *ptr, size_t new_size)
{
	void *newptr = realloc(ptr, new_size);
	if (newptr == NULL && new_size != 0)
		abort_msg("Failed to allocate %zu bytes of memory\n", new_size);

	return newptr;
}


void *malloc_safe(size_t size)
{
	return realloc_safe(NULL, size);
}
