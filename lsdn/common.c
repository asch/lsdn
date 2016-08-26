#include "common.h"

#include <stdlib.h>
#include <stdio.h>


void *realloc_safe(void *ptr, size_t new_size)
{
	void *newptr = realloc(ptr, new_size);
	if (newptr == NULL) {
		fprintf(stderr, "Failed to allocate %zu bytes of memory", new_size);
		exit(EXIT_FAILURE);
	}

	return newptr;
}


void *malloc_safe(size_t size)
{
	return realloc_safe(NULL, size);
}
