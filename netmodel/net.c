#include "private/net.h"

static void add_virt_to_bridge(
	struct lsdn_phys_attachment *a,
	struct lsdn_if *br, struct lsdn_virt *v)
{
	int err = lsdn_link_set_master(a->net->ctx->nlsock, br->ifindex, v->connected_if.ifindex);
	if(err)
		abort();
}

void lsdn_net_make_bridge(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;
	// create bridge and connect all virt interfaces to it
	struct lsdn_if bridge_if;
	lsdn_if_init_empty(&bridge_if);

	int err = lsdn_link_bridge_create(ctx->nlsock, &bridge_if, lsdn_mk_ifname(ctx));
	if(err){
		abort();
	}

	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
		add_virt_to_bridge(a, &bridge_if, v);
	}
	a->bridge_if = bridge_if;
}

// TODO code cleanup
void lsdn_net_connect_bridge(struct lsdn_phys_attachment *a)
{
	int err;
	struct lsdn_context *ctx = a->net->ctx;
	bool learning = a->net->settings->switch_type != LSDN_STATIC_E2E;

	if (learning) {
		err = lsdn_link_set_master(
			ctx->nlsock, a->bridge_if.ifindex, a->tunnel->tunnel_if.ifindex);
		if(err)
		abort();
	}

	err = lsdn_link_set(ctx->nlsock, a->tunnel->tunnel_if.ifindex, true);
	if(err)
		abort();

	lsdn_foreach(a->tunnel_list, tunnel_entry, struct lsdn_tunnel, t) {
		if (learning) {
			err = lsdn_link_set_master(
				ctx->nlsock, a->bridge_if.ifindex, t->tunnel_if.ifindex);
			if(err)
				abort();
		}

		err = lsdn_link_set(ctx->nlsock, t->tunnel_if.ifindex, true);
		if(err)
			abort();
	}

	err = lsdn_link_set(ctx->nlsock, a->bridge_if.ifindex, true);
	if(err)
		abort();

	if (!learning) {
		err = lsdn_link_set(ctx->nlsock, a->dummy_if.ifindex, true);
		if (err)
			abort();
	}
}