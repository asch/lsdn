#include "private/idalloc.h"

void lsdn_idalloc_init(struct lsdn_idalloc *idalloc, uint32_t min, uint32_t max)
{
	idalloc->min = min;
	idalloc->max = max;
	idalloc->next = min;
}

bool lsdn_idalloc_get(struct lsdn_idalloc *idalloc, uint32_t *result) {
	if (idalloc->max ==  idalloc->next)
		return false;
	*result = idalloc->next++;
	return true;
}

void lsdn_idalloc_return(struct lsdn_idalloc *idalloc, uint32_t id) {
	// TODO
}

void lsdn_idalloc_free(struct lsdn_idalloc *idalloc) {
	// TODO
}
