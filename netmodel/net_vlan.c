#include "private/net.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

static void vlan_create_pa(struct lsdn_phys_attachment *p)
{
	// create the vxlan interface
	int err = lsdn_link_vlan_create(
		p->net->ctx->nlsock, &p->tunnel.tunnel_if,
		p->phys->attr_iface, lsdn_mk_ifname(p->net->ctx), p->net->vnet_id);
	if(err)
		abort();
	lsdn_list_init_add(&p->tunnel_list, &p->tunnel.tunnel_entry);

	// register it as a bridge
	lsdn_lbridge_make(p);
	lsdn_lbridge_connect(p);
	lsdn_net_set_up(p);
}

static struct lsdn_net_ops lsdn_net_vlan_ops = {
	.create_pa = vlan_create_pa
};

struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_list_init_add(&ctx->settings_list, &s->settings_entry);
	s->ctx = ctx;
	s->ops = &lsdn_net_vlan_ops;
	s->nettype = LSDN_NET_VLAN;
	s->switch_type = LSDN_LEARNING;
	s->user_hooks = NULL;
	return s;
}
