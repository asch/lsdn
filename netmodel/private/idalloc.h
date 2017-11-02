#ifndef LSDN_IDALLOC_H
#define LSDN_IDALLOC_H

#include <stdint.h>
#include <stdbool.h>

struct lsdn_idalloc{
	uint32_t min;
	uint32_t max;
	uint32_t next;
};

void lsdn_idalloc_init(struct lsdn_idalloc *idalloc, uint32_t min, uint32_t max);
bool lsdn_idalloc_get(struct lsdn_idalloc *idalloc, uint32_t *result);
void lsdn_idalloc_return(struct lsdn_idalloc *idalloc, uint32_t id);
void lsdn_idalloc_free(struct lsdn_idalloc *idalloc);

#endif
