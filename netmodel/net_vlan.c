#include "private/net.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

static void vlan_create_pa(struct lsdn_phys_attachment *p)
{
	// create the vxlan interface
	int err = lsdn_link_vlan_create(
		p->net->ctx->nlsock, &p->tunnel_if,
		p->phys->attr_iface, lsdn_mk_ifname(p->net->ctx), p->net->vnet_id);
	if(err)
		abort();

	lsdn_lbridge_init(p->net->ctx, &p->lbridge);
	lsdn_lbridge_add(&p->lbridge, &p->lbridge_if, &p->tunnel_if);
}

static void vlan_destroy_pa(struct lsdn_phys_attachment *p)
{
	lsdn_lbridge_remove(&p->lbridge_if);
	lsdn_lbridge_free(&p->lbridge);
	if(!p->net->ctx->disable_decommit) {
		int err = lsdn_link_delete(p->net->ctx->nlsock, &p->tunnel_if);
		if (err)
			abort();
	}
	lsdn_if_free(&p->tunnel_if);
}

static struct lsdn_net_ops lsdn_net_vlan_ops = {
	.create_pa = vlan_create_pa,
	.destroy_pa = vlan_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
};

struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_settings_init_common(s, ctx);
	s->ops = &lsdn_net_vlan_ops;
	s->nettype = LSDN_NET_VLAN;
	s->switch_type = LSDN_LEARNING;
	return s;
}
