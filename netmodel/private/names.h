/** \file
 * Name-related structs and definitions. */
#pragma once

#include "list.h"
#include "../include/errors.h"

/** List of names.
 * Contains a collection of names that should be unique over their domain.
 * E.g., names of all networks (physes, virts) in a context. */
struct lsdn_names {
	/** Head of the list. */
	struct lsdn_list_entry head;
};

/** Individual name entry. */
struct lsdn_name {
	/** Name. */
	char* str;
	/** List membership. */
	struct lsdn_list_entry entry;
};

void lsdn_names_init(struct lsdn_names *tab);
void lsdn_names_free(struct lsdn_names *tab);
lsdn_err_t lsdn_name_set(struct lsdn_name *name, struct lsdn_names *table, const char* str);
void lsdn_name_init(struct lsdn_name *name);
void lsdn_name_free(struct lsdn_name *name);
struct lsdn_name * lsdn_names_search(struct lsdn_names *tab, const char* key);
// TODO: allow names removal
