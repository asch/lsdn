#include "private/lbridge.h"
#include "private/net.h"
#include "include/lsdn.h"

void lsdn_lbridge_init(struct lsdn_context *ctx, struct lsdn_lbridge *br)
{
	struct lsdn_if bridge_if;
	lsdn_if_init(&bridge_if);

	lsdn_err_t err = lsdn_link_bridge_create(ctx->nlsock, &bridge_if, lsdn_mk_ifname(ctx));
	if(err != LSDNE_OK)
		abort();

	err = lsdn_link_set(ctx->nlsock, bridge_if.ifindex, true);
	if(err != LSDNE_OK)
		abort();

	br->ctx = ctx;
	br->bridge_if = bridge_if;
}

void lsdn_lbridge_free(struct lsdn_lbridge *br)
{
	if (!br->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_link_delete(br->ctx->nlsock, &br->bridge_if);
		if (err != LSDNE_OK)
			abort();
	}
	lsdn_if_free(&br->bridge_if);
}

void lsdn_lbridge_add(struct lsdn_lbridge *br, struct lsdn_lbridge_if *br_if, struct lsdn_if *iface)
{
	lsdn_err_t err = lsdn_link_set_master(br->ctx->nlsock, br->bridge_if.ifindex, iface->ifindex);
	if(err != LSDNE_OK)
		abort();

	err = lsdn_link_set(br->ctx->nlsock, iface->ifindex, true);
	if(err != LSDNE_OK)
		abort();

	br_if->br = br;
	br_if->iface = iface;
}

void lsdn_lbridge_remove(struct lsdn_lbridge_if *iface)
{
	if (!iface->br->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_link_set_master(iface->br->ctx->nlsock, 0, iface->iface->ifindex);
		if (err != LSDNE_OK)
			abort();
	}
}

void lsdn_lbridge_add_virt(struct lsdn_virt *v)
{
	struct lsdn_phys_attachment *a = v->connected_through;
	lsdn_lbridge_add(&a->lbridge, &v->lbridge_if, &v->committed_if);
}

void lsdn_lbridge_remove_virt(struct lsdn_virt *v)
{
	lsdn_lbridge_remove(&v->lbridge_if);
}
