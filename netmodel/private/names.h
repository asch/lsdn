#ifndef _LSDN_NAMES_H
#define _LSDN_NAMES_H

#include "list.h"
#include "../include/errors.h"

struct lsdn_names {
	struct lsdn_list_entry head;
};

struct lsdn_name {
	char* str;
	struct lsdn_list_entry entry;
};

void lsdn_names_init(struct lsdn_names *tab);
lsdn_err_t lsdn_name_set(struct lsdn_name *name, struct lsdn_names *table, const char* str);
void lsdn_name_init(struct lsdn_name *name);
struct lsdn_name * lsdn_names_search(struct lsdn_names *tab, const char* key);

#endif
