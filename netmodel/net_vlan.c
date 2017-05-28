#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"

static void vlan_mktun_br(struct lsdn_phys_attachment *p)
{
	int err = lsdn_link_vlan_create(
		p->net->ctx->nlsock, &p->bridge.tunnel_if,
		p->phys->attr_iface, lsdn_mk_ifname(p->net->ctx), p->net->vlan_id);
	if(err)
		abort();
}

struct lsdn_net_ops lsdn_net_vlan_ops = {
	.mktun_br = vlan_mktun_br
};
