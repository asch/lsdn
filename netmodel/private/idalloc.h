/** \file
 * ID allocation related structs and definitions. */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/** ID allocation record. */
struct lsdn_idalloc{
	/** Starting ID. */
	uint32_t min;
	/** Maximum allowable ID. */
	uint32_t max;
	/** Next ID to allocate. */
	uint32_t next;
};

void lsdn_idalloc_init(struct lsdn_idalloc *idalloc, uint32_t min, uint32_t max);
bool lsdn_idalloc_get(struct lsdn_idalloc *idalloc, uint32_t *result);
void lsdn_idalloc_return(struct lsdn_idalloc *idalloc, uint32_t id);
void lsdn_idalloc_free(struct lsdn_idalloc *idalloc);
