#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "include/nettypes.h"
#include "private/errors.h"

static void vxlan_mcast_mktun_br(struct lsdn_phys_attachment *a)
{
	struct lsdn_settings *s = a->net->settings;
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		&s->vxlan.mcast.mcast_ip,
		a->net->vnet_id,
		s->vxlan.port,
		true);
	if (err)
		abort();
	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.mktun_br = vxlan_mcast_mktun_br
};

struct lsdn_settings *lsdn_settings_new_vxlan_mcast(
	struct lsdn_context *ctx,
	lsdn_ip_t mcast_ip, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_list_init_add(&ctx->settings_list, &s->settings_entry);
	s->ctx = ctx;
	s->ops = &lsdn_net_vxlan_mcast_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = LSDN_LEARNING;
	s->vxlan.mcast.mcast_ip = mcast_ip;
	s->vxlan.port = port;
	return s;
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
			*v->attr_mac, *a_other->phys->attr_ip);
		if (err)
			abort();
	}
}

static void vxlan_e2e_mktun_br(struct lsdn_phys_attachment *a)
{
	bool learning = a->net->settings->switch_type == LSDN_LEARNING_E2E;
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port,
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
			!learning ? lsdn_broadcast_mac : lsdn_all_zeroes_mac,
			*a_other->phys->attr_ip);
		if (err)
			abort();
	}

	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);
}

struct lsdn_net_ops lsdn_net_vxlan_e2e_ops = {
	.mktun_br = vxlan_e2e_mktun_br
};

static struct lsdn_settings *new_e2e(struct lsdn_context *ctx, uint16_t port, enum lsdn_switch stype)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_list_init_add(&ctx->settings_list, &s->settings_entry);
	s->ctx = ctx;
	s->ops = &lsdn_net_vxlan_e2e_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = stype;
	s->vxlan.port = port;
	return s;
}

struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port)
{
	return new_e2e(ctx, port, LSDN_LEARNING_E2E);
}

struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port)
{
	return new_e2e(ctx, port, LSDN_STATIC_E2E);
}
