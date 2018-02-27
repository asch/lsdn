/** \file
 * Error return helpers. */
#pragma once

#include "../include/errors.h"
#include "../include/lsdn.h"
#include "lsdn.h"

void lsdn_problem_report(struct lsdn_context *ctx, enum lsdn_problem_code code, ...);

/** Return error code and/or call OoM callback.
 * Checks if the error is #LSDNE_NOMEM and invokes `lsdn_context.nomem_cb`
 * if specified. Then it returns the error.
 * @param ctx LSDN context.
 * @param err error code.
 * @return value of `err`. */
static inline lsdn_err_t ret_err(struct lsdn_context *ctx, lsdn_err_t err)
{
	if(err == LSDNE_NOMEM && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return err;
}

/** Return error code here. */
#define ret_err(ctx, x) return ret_err(ctx, x)

/** Return pointer and/or call OoM callback.
 * Checks if the pointer is `NULL`. If yes, assumes that it was caused
 * by a failed allocations and invokes `lsdn_context.nomem_cb` if specified.
 * Then it returns the pointer.
 * @param ctx LSDN context.
 * @param ptr pointer to check.
 * @return value of `ptr`. */
static inline void *ret_ptr(struct lsdn_context *ctx, void *ptr){
	if(ptr == NULL && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return ptr;
}

static inline bool mark_commit_err(
	struct lsdn_context *ctx, enum lsdn_state *s, enum lsdn_problem_ref_type type, void *subj, bool fatal, lsdn_err_t err)
{
	if (*s == LSDN_STATE_FAIL)
		return true;

	if (err == LSDNE_INCONSISTENT || (fatal && err == LSDNE_NETLINK)) {
		*s = LSDN_STATE_FAIL;
		lsdn_problem_report(ctx, LSDNP_COMMIT_NETLINK_CLEANUP, type, subj, LSDNS_END);
		return true;
	} else if (err == LSDNE_NETLINK) {
		*s = LSDN_STATE_ERR;
		lsdn_problem_report(ctx, LSDNP_COMMIT_NETLINK, type, subj, LSDNS_END);
		return true;
	} else if (err == LSDNE_NOMEM) {
		*s = LSDN_STATE_ERR;
		lsdn_problem_report(ctx, LSDNP_COMMIT_NOMEM, type, subj, LSDNS_END);
		return true;
	} else if (err == LSDNE_OK) {
		return false;
	} else {
		abort();
	}
}

static inline bool state_ok(enum lsdn_state state)
{
	return state == LSDN_STATE_OK || state == LSDN_STATE_NEW;
}

static inline void acc_inconsistent(lsdn_err_t *dst, lsdn_err_t src)
{
	if (src != LSDNE_OK)
		*dst = LSDNE_INCONSISTENT;
}

/** Return pointer here. */
#define ret_ptr(ctx, x) return ret_ptr(ctx, x)
