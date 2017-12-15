/** \file
 * Common functions for various types of networks. */
#include "private/net.h"

/** Make a unique interface name.
 * Creates a new interface name for a given context. The name is in the form
 * `"ctxname-12"`, where "ctxname" is `name` for the context and "12" is number
 * of already created interfaces.
 * The generated name is stored in `namebuf` field of `ctx`, so repeated calls
 * to `lsdn_mk_ifname` overwrite it. You should make a private copy if you need it.
 * @param ctx LSDN context.
 * @return Pointer to the name string. */
const char *lsdn_mk_ifname(struct lsdn_context* ctx)
{
	snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s-%d", ctx->name, ++ctx->obj_count);
	return ctx->namebuf;
}

/** Initialize common parts of `lsdn_settings` struct. */
lsdn_err_t lsdn_settings_init_common(struct lsdn_settings *settings, struct lsdn_context *ctx)
{
	lsdn_name_init(&settings->name);
	lsdn_err_t err = lsdn_name_set(&settings->name, &ctx->setting_names, lsdn_mk_settings_name(ctx));
	if (err != LSDNE_OK)
		return err;
	settings->state = LSDN_STATE_NEW;
	lsdn_list_init(&settings->setting_users_list);
	settings->user_hooks = NULL;
	lsdn_list_init_add(&ctx->settings_list, &settings->settings_entry);
	settings->ctx = ctx;
	return LSDNE_OK;
}

/** Initialize ruleset engine.
 * TODO */
lsdn_err_t lsdn_prepare_rulesets(
	struct lsdn_context *ctx, struct lsdn_if *iface,
	struct lsdn_ruleset* in, struct lsdn_ruleset* out)
{
	lsdn_err_t err;
	if (out) {
		err = lsdn_qdisc_egress_create(ctx->nlsock, iface->ifindex);
		if (err != LSDNE_OK)
			return err;

		lsdn_ruleset_init(
			out, ctx, iface,
			LSDN_ROOT_HANDLE, LSDN_DEFAULT_CHAIN, 1, UINT32_MAX);
	}

	if (in) {
		err = lsdn_qdisc_ingress_create(ctx->nlsock, iface->ifindex);
		if (err != LSDNE_OK)
			abort();

		lsdn_ruleset_init(
			in, ctx, iface,
			LSDN_INGRESS_HANDLE, LSDN_DEFAULT_CHAIN, 1, UINT32_MAX);
	}

	return LSDNE_OK;
}
