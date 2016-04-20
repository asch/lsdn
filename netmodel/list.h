#ifndef _LSDN_LIST_H
#define _LSDN_LIST_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

/* Also used for the list head */
struct lsdn_list_entry {
	struct lsdn_list_entry *next;
	struct lsdn_list_entry *previous;
};
#define lsdn_container_of(ptr, type, member) \
	( (type *)((uint8_t *)(ptr) - offsetof(type, member)) )

#define lsdn_foreach_list(list, member, type, name) \
	for(type* name = lsdn_container_of(list.next, type, member); \
	    &name->member != &list; \
	    name = lsdn_container_of(name->member.next, type, member)) \

void lsdn_list_init(struct lsdn_list_entry *head);
void lsdn_list_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry);
void lsdn_list_remove(struct lsdn_list_entry *entry);
int lsdn_is_list_empty(struct lsdn_list_entry *head);

typedef int (*lsdn_predicate_t)(struct lsdn_list_entry *what, void *ctx);



#endif
