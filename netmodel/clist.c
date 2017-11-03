/** \file
 * Cleanup list management routines.
 */
#include "private/clist.h"

/** Initialize a cleanup list. */
void lsdn_clist_init(struct lsdn_clist *clist, size_t clist_index)
{
	lsdn_list_init(&clist->cleanup_list);
	clist->clist_index = clist_index;
}

/** Set up a cleanup list entry. */
void lsdn_clist_init_entry(struct lsdn_clist_entry *entry, lsdn_clist_cb cb, void *user)
{
	entry->cb = cb;
	entry->user = user;
	for(size_t i = 0; i<LSDN_CLIST_MAX; i++) {
		lsdn_list_init(&entry->cleanup_entry[i]);
	}
}

/** Insert an entry into a cleanup list. */
void lsdn_clist_add(struct lsdn_clist *clist, struct lsdn_clist_entry *entry)
{
	struct lsdn_list_entry *e = &entry->cleanup_entry[clist->clist_index];
	assert (lsdn_is_list_empty(e));
	lsdn_list_add(&clist->cleanup_list, e);
}

/** Perform cleanup callbacks on the list.
 * This function traverses the cleanup list, dropping all the entries
 * and invoking their callbacks.
 */
void lsdn_clist_flush(struct lsdn_clist *clist)
{
	lsdn_foreach(clist->cleanup_list, cleanup_entry[clist->clist_index], struct lsdn_clist_entry, ce) {
		for(size_t i = 0; i<LSDN_CLIST_MAX; i++) {
			struct lsdn_list_entry *e = &ce->cleanup_entry[i];
			if (!lsdn_is_list_empty(e))
				lsdn_list_remove(e);
		}
		ce->cb(ce->user);
	}
}
