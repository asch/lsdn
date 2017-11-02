#include "private/net.h"

const char *lsdn_mk_ifname(struct lsdn_context* ctx)
{
	snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s-%d", ctx->name, ++ctx->ifcount);
	return ctx->namebuf;
}

void lsdn_settings_init_common(struct lsdn_settings *settings, struct lsdn_context *ctx)
{
	settings->state = LSDN_STATE_NEW;
	lsdn_list_init(&settings->setting_users_list);
	settings->user_hooks = NULL;
	lsdn_list_init_add(&ctx->settings_list, &settings->settings_entry);
	settings->ctx = ctx;
}
