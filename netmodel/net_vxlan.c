#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"

static void vxlan_mcast_mktun_br(struct lsdn_phys_attachment *a)
{
	int err = lsdn_link_vxlan_mcast_create(
		a->net->ctx->nlsock,
		&a->bridge.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		a->net->vxlan_mcast.mcast_ip,
		a->net->vxlan_mcast.vxlan_id,
		a->net->vxlan_mcast.port);
	if (err)
		abort();
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.mktun_br = vxlan_mcast_mktun_br
};
