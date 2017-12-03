/** \file
 * Linked list management structs, definitions and macros. 
 * TODO explain linked lists here? */
#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

/** List entry.
 * Used as a member in another struct, allowing that struct to be
 * an entry in a linked list.
 *
 * Also used as a list head, or a pointer to the list as a whole. */
struct lsdn_list_entry {
	struct lsdn_list_entry *next;
	struct lsdn_list_entry *previous;
};

/** Get a pointer to the container of a given list entry.
 * When walking the list defined by `lsdn_list_entry`, this returns
 * the struct that contains the `lsdn_list_entry` in question.
 * @param ptr pointer to `lsdn_list_entry`.
 * @param type type of the containing struct.
 * @param member name of the relevant `lsdn_list_entry` member.
 * @return typed pointer to the containing struct. */
#define lsdn_container_of(ptr, type, member) \
	( (type *)((uint8_t *)(ptr) - offsetof(type, member)) )

/** Walk the linked list.
 * Declares `for` cycle iterating over all entries in a given linked list.
 * @param list reference (not pointer) to `lsdn_list_entry` head of the list.
 * @param member name of the `lsdn_list_entry` member for this list.
 * @param type type of the contents of the list.
 * @param name name of the iterating variable.
 *
 * TODO example. */
#define lsdn_foreach(list, member, type, name) \
	for(type *name = lsdn_container_of(list.next, type, member), \
	    *name##_next = lsdn_container_of(name->member.next, type, member); \
	    &name->member != &list; \
	    name = name##_next, name##_next = lsdn_container_of(name->member.next, type, member)) \

void lsdn_list_init(struct lsdn_list_entry *head);
void lsdn_list_init_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry);
void lsdn_list_add(struct lsdn_list_entry *position, struct lsdn_list_entry *entry);
void lsdn_list_remove(struct lsdn_list_entry *entry);
int lsdn_is_list_empty(struct lsdn_list_entry *head);

/** Unused TODO remove? */
typedef int (*lsdn_predicate_t)(struct lsdn_list_entry *what, void *ctx);
