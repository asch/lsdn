/** \file
 * Linked list management routines.
 */
#include "private/list.h"

/** Set up the linked list head. */
void lsdn_list_init(struct lsdn_list_entry *head)
{
	head->next = head;
	head->previous = head;
}

/** Insert an entry into the list.
 *
 * Both the position and entry must be already initialized using lsdn_list_init
 * and the entry must not already be member of a list. Prefer this function to
 * lsdn_list_init_add if objects change list membership during their lifetime
 * (it has assert for checking that you have removed the object from previous
 * list).
 */
void lsdn_list_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry)
{
	/* Since the entry is initialized, we can check if it is empty */
	assert(lsdn_is_list_empty(entry));
	lsdn_list_init_add(position, entry);
}

/** Initialize an entry and insert it into the list.
 *
 * The position must be already initialized.
 */
void lsdn_list_init_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry)
{
	entry->previous = position;
	entry->next = position->next;
	position->next = entry;
	entry->next->previous = entry;
}

/** Unlink an entry from the list. */
void lsdn_list_remove(struct lsdn_list_entry *entry)
{
	assert(!lsdn_is_list_empty(entry));
	entry->previous->next = entry->next;
	entry->next->previous = entry->previous;
	lsdn_list_init(entry);
}

/** Check if the list is empty, i.e., it only contains its head. */
int lsdn_is_list_empty(struct lsdn_list_entry *head)
{
	return head->next == head;
}
