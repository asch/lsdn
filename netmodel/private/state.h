/** \file
 * Commit state related definitions. */
#pragma once
#include <stdbool.h>
#include "../include/errors.h"
#include <assert.h>

/** Commit state of the LSDN object. */
enum lsdn_state {
	/** Object is being committed for a first time. */
	LSDN_STATE_NEW,
	/** Object was already committed and needs to be recommitted. */
	LSDN_STATE_RENEW,
	/** Object is already committed and needs to be deleted. */
	LSDN_STATE_DELETE,
	/** Object is in committed state. */
	LSDN_STATE_OK,
	/** A temporary state during commit for objects where creation has failed */
	LSDN_STATE_ERR,
	LSDN_STATE_FAIL,
};

/** Mark object for deletion.
 * If the object is in NEW state, so it's not in tc tables yet, delete it
 * immediately. Otherwise, set to DELETE state for later deletion sweep.
 * @param obj Object to delete, expected to have a `state` member.
 * @param freefn Deallocation function. */
#define free_helper(obj, freefn) \
	do{ \
		if (obj->state == LSDN_STATE_NEW) { \
			freefn(obj); \
		} else { \
			obj->pending_free = true; \
			obj->state = LSDN_STATE_DELETE; \
		}\
	} while(0)

/** Mark object as commited.
 * If the state was `LSDN_STATE_NEW`
 * or `LSDN_STATE_RENEW`, that means the object is now commited.
 * The new state is `LSDN_STATE_OK`.
 *
 * If the state was already `OK`, no change. If the state was `LSDN_STATE_DELETE`,
 * the object is now deleted, so `LSDN_STATE_DELETE` is the only possible appropriate
 * value.
 *
 * @param s Pointer to commit state. */
static inline void ack_state(enum lsdn_state *s)
{
	if (*s == LSDN_STATE_NEW || *s == LSDN_STATE_RENEW)
		*s = LSDN_STATE_OK;
	if (*s == LSDN_STATE_ERR)
		*s = LSDN_STATE_NEW;
}

/** Help process object deletion or renew.
 * If the object was in `LSDN_STATE_DELETE`, the state will not change (the whole object will be
 * deleted later instead, use ack_delete for that).  If the object was in `LSDN_STATE_RENEW`,
 * it must be reinstalled, so the state is set to `LSDN_STATE_NEW`. Does nothing for other states.
 *
 * @param s Pointer to commit state.
 * @retval true The caller should decommit the object, because it is getting renewed or deleted.
 * @retval false The object was in other state.
 */
static inline bool ack_decommit(enum lsdn_state *s)
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
 * If the object is marked with `pending_free`, deallocate it.
 * @param obj Object to free.
 * @param freefn Deallocator function. */
#define ack_delete(obj, freefn) \
	do { \
		if (obj->pending_free) \
			freefn(obj); \
	} while(0)
