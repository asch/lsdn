#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "include/nettypes.h"

static void vxlan_mcast_mktun_br(struct lsdn_phys_attachment *a)
{
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->bridge.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		&a->net->vxlan_mcast.mcast_ip,
		a->net->vxlan_mcast.vxlan_id,
		a->net->vxlan_mcast.port);
	if (err)
		abort();
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.mktun_br = vxlan_mcast_mktun_br
};

struct lsdn_net *lsdn_net_new_vxlan_mcast(
	struct lsdn_context *ctx, uint32_t vxlan_id,
	lsdn_ip_t mcast_ip, uint16_t port)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		return NULL;
	net->ctx = ctx;
	net->switch_type = LSDN_LEARNING;
	net->nettype = LSDN_NET_VXLAN;
	net->ops = &lsdn_net_vxlan_mcast_ops;
	net->vxlan_mcast.vxlan_id = vxlan_id;
	net->vxlan_mcast.mcast_ip = mcast_ip;
	net->vxlan_mcast.port = port;
	lsdn_list_init_add(&ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	return net;
}

static void vxlan_e2e_mktun_br(struct lsdn_phys_attachment *a)
{
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->bridge.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vxlan_e2e.vxlan_id,
		a->net->vxlan_e2e.port);
	if (err)
		abort();

	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other) {
		if (&a_other->phys->phys_entry == &a->phys->phys_entry)
			continue;
		err = lsdn_fdb_add_entry(a->net->ctx->nlsock, a->bridge.tunnel_if.ifindex,
					&lsdn_broadcast_mac, a_other->phys->attr_ip);
		if (err)
			abort();
	}
}

struct lsdn_net_ops lsdn_net_vxlan_e2e_ops = {
	.mktun_br = vxlan_e2e_mktun_br
};

struct lsdn_net *lsdn_net_new_vxlan_e2e(
	struct lsdn_context *ctx, uint32_t vxlan_id, uint16_t port)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		return NULL;
	net->ctx = ctx;
	net->switch_type = LSDN_LEARNING_E2E;
	net->nettype = LSDN_NET_VXLAN;
	net->ops = &lsdn_net_vxlan_e2e_ops;
	net->vxlan_e2e.vxlan_id = vxlan_id;
	net->vxlan_e2e.port = port;
	lsdn_list_init_add(&ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	return net;
}
