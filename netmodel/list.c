#include "private/list.h"

void lsdn_list_init(struct lsdn_list_entry *head)
{
	head->next = head;
	head->previous = head;
}

void lsdn_list_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry){
	assert(lsdn_is_list_empty(entry));

	entry->previous = position;
	entry->next = position->next;
	position->next = entry;
	entry->next->previous = entry;
}

void lsdn_list_remove(struct lsdn_list_entry *entry){
	assert(!lsdn_is_list_empty(entry));
	entry->previous->next = entry->next;
	entry->next->previous = entry->previous;
	lsdn_list_init(entry);
}
int lsdn_is_list_empty(struct lsdn_list_entry *head)
{
	return head->next == head;
}
