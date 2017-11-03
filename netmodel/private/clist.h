/** \file
 * cleanup list related structs and function headers
 */
#pragma once

#include "list.h"

typedef void (*lsdn_clist_cb)(void *user);

/** Maximum number of cleanup lists for an entry to inhabit simultaneously. */
#define LSDN_CLIST_MAX 2

/** Cleanup list.
 * Allows objects to register cleanup handlers to be called when the
 * object is destroyed.
 */
struct lsdn_clist{
	struct lsdn_list_entry cleanup_list;
	size_t clist_index;
};

/** Cleanup list entry.
 * Each entry can be in multiple lists, as long as they have
 * different indices.
 */
struct lsdn_clist_entry {
	struct lsdn_list_entry cleanup_entry[LSDN_CLIST_MAX];
	lsdn_clist_cb cb;
	void *user;
};

void lsdn_clist_init(struct lsdn_clist *clist, size_t clist_index);
void lsdn_clist_init_entry(struct lsdn_clist_entry *entry, lsdn_clist_cb cb, void *user);
void lsdn_clist_add(struct lsdn_clist *clist, struct lsdn_clist_entry *entry);
void lsdn_clist_flush(struct lsdn_clist *clist);
