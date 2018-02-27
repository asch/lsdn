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

/** Insert an entry into the list. */
/* TODO do we really have a function that just checks the assert
 * and otherwise does the SAME THING? */
void lsdn_list_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry)
{
	assert(lsdn_is_list_empty(entry));
	lsdn_list_init_add(position, entry);
}

/** Really insert an entry into the list. */
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
