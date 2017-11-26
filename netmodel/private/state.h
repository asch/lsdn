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

static inline void ack_state(enum lsdn_state *s)
{
	if (*s == LSDN_STATE_NEW || *s == LSDN_STATE_RENEW)
		*s = LSDN_STATE_OK;
}

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

#define ack_delete(obj, free) { \
		if (obj->state == LSDN_STATE_DELETE) \
			free(obj); \
	}while(0);
