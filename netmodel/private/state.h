/** \file
 * State-related definitions. */
#pragma once
#include <stdbool.h>

/** State of the LSDN object. */
enum lsdn_state {
	/** Object is being committed for a first time. */
	LSDN_STATE_NEW,
	/** Object was already committed and needs to be recommitted. */
	LSDN_STATE_RENEW,
	/** Object is already committed and needs to be deleted. */
	LSDN_STATE_DELETE,
	/** Object is in committed state. */
	LSDN_STATE_OK
};

/** Mark object for deletion.
 * If the object is in NEW state, so it's not in tc tables yet, delete it
 * immediately. Otherwise, set to DELETE state for later deletion sweep.
 * @param obj Object to delete, expected to have a `state` member.
 * @param free Deletion function to use. */
#define free_helper(obj, free) \
	do{ \
		if (obj->state == LSDN_STATE_NEW) \
			free(obj); \
		else \
			obj->state = LSDN_STATE_DELETE; \
	} while(0)

/** Mark object as commited.
 * Called when rules for this object are processed. If the state was `LSDN_STATE_NEW`
 * or `LSDN_STATE_RENEW`, that means the object is now commited.
 * The new state is `LSDN_STATE_OK`.
 *
 * If the state was already `OK`, no change. If the state was `LSDN_STATE_DELETE`,
 * the object is now deleted, so `LSDN_STATE_DELETE` is the only possible appropriate
 * value. */
static inline void ack_state(enum lsdn_state *s)
{
	if (*s == LSDN_STATE_NEW || *s == LSDN_STATE_RENEW)
		*s = LSDN_STATE_OK;
}

/** Mark object as deleted. 
 * Called when rules for this object are deleted.
 * If the object was in `LSDN_STATE_DELETE`, that was the intended action is done.
 *
 * If the object was in `LSDN_STATE_RENEW`, it must be reinstalled, so the state
 * is set to `LSDN_STATE_NEW`.
 *
 * @return `true` if the object should now be freed, `false` otherwise */
static inline bool ack_uncommit(enum lsdn_state *s)
{
	switch(*s) {
	case LSDN_STATE_DELETE:
		return true;
	case LSDN_STATE_RENEW:
		*s = LSDN_STATE_NEW;
		return true;
	default:
		return false;
	}
}

/** Free a deleted object.
 * If the state is `LSDN_STATE_DELETE`, call `free` on the object.
 * @param obj object to free
 * @param free deallocator function */
#define ack_delete(obj, free) \
	do { \
		if (obj->state == LSDN_STATE_DELETE) \
			free(obj); \
	} while(0)
