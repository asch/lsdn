#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"

static void vlan_mktun_br(struct lsdn_phys_attachment *p)
{
	int err = lsdn_link_vlan_create(
		p->net->ctx->nlsock, &p->tunnel.tunnel_if,
		p->phys->attr_iface, lsdn_mk_ifname(p->net->ctx), p->net->vlan_id);
	if(err)
		abort();
	lsdn_list_init_add(&p->tunnel_list, &p->tunnel.tunnel_entry);
}

static struct lsdn_net_ops lsdn_net_vlan_ops = {
	.mktun_br = vlan_mktun_br
};

struct lsdn_net *lsdn_net_new_vlan(struct lsdn_context *ctx, uint32_t vlan_id)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		return NULL;
	net->ctx = ctx;
	net->switch_type = LSDN_LEARNING;
	net->nettype = LSDN_NET_VLAN;
	net->ops = &lsdn_net_vlan_ops;
	net->vlan_id = vlan_id;
	lsdn_list_init_add(&ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	return net;
}
