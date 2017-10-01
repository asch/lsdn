#include "private/net.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

static void direct_create_pa(struct lsdn_phys_attachment *a)
{
	// register the interface directly as tunnel
	lsdn_err_t err;
	err = lsdn_if_init_name(&a->tunnel.tunnel_if, a->phys->attr_iface);
	if (err != LSDNE_OK)
		abort();
	err = lsdn_if_prepare(&a->tunnel.tunnel_if);
	if (err != LSDNE_OK)
		abort();
	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);

	// create and acivate the bridge
	lsdn_lbridge_make(a);
	lsdn_lbridge_connect(a);
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
