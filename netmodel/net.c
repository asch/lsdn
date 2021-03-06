/** \file
 * Common functions for various types of networks. */
#include "private/net.h"
#include "private/errors.h"

/** Initialize common parts of #lsdn_settings struct. */
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

/** Initialize ruleset engine. */
lsdn_err_t lsdn_prepare_rulesets(
	struct lsdn_context *ctx, struct lsdn_if *iface,
	struct lsdn_ruleset* in, struct lsdn_ruleset* out)
{
	lsdn_err_t err;
	if (out) {
		err = lsdn_qdisc_egress_create(ctx->nlsock, iface->ifindex, ctx->overwrite);
		if (err != LSDNE_OK)
			return err;
		lsdn_ruleset_init(
			out, ctx, iface,
			LSDN_ROOT_HANDLE, LSDN_DEFAULT_CHAIN, 1, UINT32_MAX);
	}

	if (in) {
		err = lsdn_qdisc_ingress_create(ctx->nlsock, iface->ifindex, ctx->overwrite);
		if (err != LSDNE_OK)
			abort();
		lsdn_ruleset_init(
			in, ctx, iface,
			LSDN_INGRESS_HANDLE, LSDN_DEFAULT_CHAIN, 1, UINT32_MAX);
	}

	return LSDNE_OK;
}

lsdn_err_t lsdn_cleanup_rulesets(
	struct lsdn_context *ctx, struct lsdn_if *iface,
	struct lsdn_ruleset* in, struct lsdn_ruleset* out)
{
	lsdn_err_t err = LSDNE_OK;
	if (out) {
		lsdn_ruleset_free(out);
		if (!ctx->disable_decommit)
			acc_inconsistent(&err, lsdn_qdisc_egress_delete(ctx->nlsock, iface->ifindex));
	}
	if (in) {
		lsdn_ruleset_free(in);
		if (!ctx->disable_decommit)
			acc_inconsistent(&err, lsdn_qdisc_ingress_delete(ctx->nlsock, iface->ifindex));

	}
	return err;
}
