#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "include/nettypes.h"

static void vxlan_mcast_mktun_br(struct lsdn_phys_attachment *a)
{
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		&a->net->vxlan.mcast.mcast_ip,
		a->net->vxlan.vxlan_id,
		a->net->vxlan.port,
		true);
	if (err)
		abort();
	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.mktun_br = vxlan_mcast_mktun_br
};

struct lsdn_net *lsdn_net_new_vxlan_mcast(
	struct lsdn_context *ctx, uint32_t vxlan_id,
	lsdn_ip_t mcast_ip, uint16_t port)
{
	struct lsdn_net *net = lsdn_net_new_common(ctx, LSDN_NET_VXLAN, LSDN_LEARNING);
	if(!net)
		return NULL;
	net->ops = &lsdn_net_vxlan_mcast_ops;
	net->vxlan.vxlan_id = vxlan_id;
	net->vxlan.mcast.mcast_ip = mcast_ip;
	net->vxlan.port = port;
	return net;
}

static void fdb_fill_virts(struct lsdn_phys_attachment *a, struct lsdn_phys_attachment *a_other)
{
	int err;
	lsdn_foreach(
		a_other->connected_virt_list, connected_virt_entry,
		struct lsdn_virt, v)
	{
		if(!v->attr_mac)
			continue;

		// TODO: add validation to check that the mac is known and report errors
		err = lsdn_fdb_add_entry(
			a->net->ctx->nlsock, a->tunnel.tunnel_if.ifindex,
			v->attr_mac, a_other->phys->attr_ip);
		if (err)
			abort();
	}
}

static void vxlan_e2e_mktun_br(struct lsdn_phys_attachment *a)
{
	bool learning = a->net->switch_type == LSDN_LEARNING_E2E;
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vxlan.vxlan_id,
		a->net->vxlan.port,
		learning);
	if (err)
		abort();

	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other)
	{
		if (&a_other->phys == &a->phys)
			continue;
		if(!learning)
			fdb_fill_virts(a, a_other);

		//TODO: all zeros
		err = lsdn_fdb_add_entry(
			a->net->ctx->nlsock, a->tunnel.tunnel_if.ifindex,
			!learning ? &lsdn_broadcast_mac : &lsdn_all_zeroes_mac,
			a_other->phys->attr_ip);
		if (err)
			abort();
	}

	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);
}

struct lsdn_net_ops lsdn_net_vxlan_e2e_ops = {
	.mktun_br = vxlan_e2e_mktun_br
};

static struct lsdn_net *new_e2e(
	struct lsdn_context *ctx, uint32_t vxlan_id, uint16_t port, enum lsdn_switch stype)
{
	struct lsdn_net *net = lsdn_net_new_common(ctx, LSDN_NET_VXLAN, stype);
	if(!net)
		return NULL;
	net->ops = &lsdn_net_vxlan_e2e_ops;
	net->vxlan.vxlan_id = vxlan_id;
	net->vxlan.port = port;
	return net;
}

struct lsdn_net *lsdn_net_new_vxlan_e2e(
	struct lsdn_context *ctx, uint32_t vxlan_id, uint16_t port)
{
	return new_e2e(ctx, vxlan_id, port, LSDN_LEARNING_E2E);
}

struct lsdn_net *lsdn_net_new_vxlan_static(
	struct lsdn_context *ctx, uint32_t vxlan_id, uint16_t port)
{
	return new_e2e(ctx, vxlan_id, port, LSDN_STATIC_E2E);
}
