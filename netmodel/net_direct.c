#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

static void direct_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_net_make_bridge(a);

	int ifindex = if_nametoindex(a->phys->attr_iface);
	int err = lsdn_link_set_master(a->net->ctx->nlsock, a->bridge_if.ifindex, ifindex);
	if(err)
		abort();

	lsdn_net_set_up(a);
}

static struct lsdn_net_ops lsdn_net_direct_ops = {
	.create_pa = direct_create_pa
};

struct lsdn_settings *lsdn_settings_new_direct(struct lsdn_context *ctx)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_list_init_add(&ctx->settings_list, &s->settings_entry);
	s->ctx = ctx;
	s->ops = &lsdn_net_direct_ops;
	s->nettype = LSDN_NET_DIRECT;
	s->switch_type = LSDN_LEARNING;
	return s;
}
