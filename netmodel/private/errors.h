/** \file
 * Error return helpers. */
#pragma once

#include "../include/errors.h"
#include "../include/lsdn.h"
#include "lsdn.h"

/** Return error code and/or call OoM callback.
 * Checks if the error is #LSDNE_NOMEM and invokes `lsdn_context.nomem_cb`
 * if specified. Then it returns the error. */
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
 * Then it returns the pointer. */
static inline void *ret_ptr(struct lsdn_context *ctx, void *ptr){
	if(ptr == NULL && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return ptr;
}
/** Return pointer here. */
#define ret_ptr(ctx, x) return ret_ptr(ctx, x)
