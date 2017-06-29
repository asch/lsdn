#ifndef LSDN_PRIVATE_ERRORS_H
#define LSDN_PRIVATE_ERRORS_H

#include "../include/errors.h"
#include "../include/lsdn.h"

static inline lsdn_err_t ret_err(struct lsdn_context *ctx, lsdn_err_t err)
{
	if(err == LSDNE_NOMEM && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return err;
}
#define ret_err(ctx, x) return ret_err(ctx, x)

static inline void *ret_ptr(struct lsdn_context *ctx, void *ptr){
	if(ptr == NULL && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return ptr;
}
#define ret_ptr(ctx, x) return ret_ptr(ctx, x)

#endif
