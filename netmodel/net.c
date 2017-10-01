#include "private/net.h"

const char *lsdn_mk_ifname(struct lsdn_context* ctx)
{
	snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s-%d", ctx->name, ++ctx->ifcount);
	return ctx->namebuf;
}

void lsdn_net_set_up(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;
	int err;

	err = lsdn_link_set(ctx->nlsock, a->bridge_if.ifindex, true);
	if(err)
		abort();

	lsdn_foreach(a->tunnel_list, tunnel_entry, struct lsdn_tunnel, t) {
		err = lsdn_link_set(ctx->nlsock, t->tunnel_if.ifindex, true);
		if(err)
			abort();
	}
}
