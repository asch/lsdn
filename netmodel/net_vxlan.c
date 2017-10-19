#include "private/net.h"
#include "private/sbridge.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "include/nettypes.h"
#include "private/errors.h"
#include <stdarg.h>

static void vxlan_mcast_create_pa(struct lsdn_phys_attachment *a)
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
		true,
		false);
	if (err)
		abort();

	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);

	lsdn_lbridge_make(a);
	lsdn_lbridge_connect(a);
	lsdn_net_set_up(a);
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.create_pa = vxlan_mcast_create_pa
};

struct lsdn_settings *lsdn_settings_new_vxlan_mcast(
	struct lsdn_context *ctx,
	lsdn_ip_t mcast_ip, uint16_t port)
{
	if(port == 0)
		port = 4789;

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
	s->state = LSDN_STATE_NEW;
	return s;
}

static void fdb_fill_virts(struct lsdn_phys_attachment *a, struct lsdn_phys_attachment *a_other)
{
	int err;
	// TODO: use a similar approach we use in flower.h for the fdb
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

static void vxlan_e2e_create_pa(struct lsdn_phys_attachment *a)
{
	// TODO: maybe it is usefull to have e2e using FDB without learning?
	// provide a settings constructor to do that
	bool learning = a->net->settings->switch_type == LSDN_LEARNING_E2E;
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port,
		false,
		false);
	if (err)
		abort();

	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);


	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other)
	{
		if (&a_other->phys == &a->phys)
			continue;
		if(!learning)
			fdb_fill_virts(a, a_other);

		err = lsdn_fdb_add_entry(
			a->net->ctx->nlsock, a->tunnel.tunnel_if.ifindex,
			!learning ? lsdn_broadcast_mac : lsdn_all_zeroes_mac,
			*a_other->phys->attr_ip);
		if (err)
			abort();
	}

	lsdn_lbridge_make(a);
	lsdn_lbridge_connect(a);
	lsdn_net_set_up(a);
}

struct lsdn_net_ops lsdn_net_vxlan_e2e_ops = {
	.create_pa = vxlan_e2e_create_pa
};


struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_list_init_add(&ctx->settings_list, &s->settings_entry);
	s->ctx = ctx;
	s->ops = &lsdn_net_vxlan_e2e_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = LSDN_LEARNING_E2E;
	s->vxlan.port = port;
	s->state = LSDN_STATE_NEW;
	return s;
}

/* Make sure the VXLAN interface operating in metadata mode for that UDP port exists. */
static void vxlan_init_static_tunnel(struct lsdn_settings *s)
{
	int err;
	struct lsdn_context *ctx = s->ctx;
	struct lsdn_shared_tunnel *tun = &s->vxlan.e2e_static.tunnel;
	if (tun->refcount == 0) {
		lsdn_idalloc_init(&tun->chain_ids, 1, 0xFFFF);
		err = lsdn_link_vxlan_create(
			ctx->nlsock,
			&tun->tunnel_if,
			NULL,
			lsdn_mk_ifname(ctx),
			NULL,
			0,
			s->vxlan.port,
			false,
			true);
		if (err)
			abort();

		err = lsdn_qdisc_ingress_create(ctx->nlsock, tun->tunnel_if.ifindex);
		if (err)
			abort();

		err = lsdn_link_set(ctx->nlsock, tun->tunnel_if.ifindex, true);
		if (err)
			abort();
	}
	tun->refcount++;
}

static void vxlan_static_create_pa(struct lsdn_phys_attachment *pa)
{
	vxlan_init_static_tunnel(pa->net->settings);
	lsdn_sbridge_init(pa->net->ctx, &pa->sbridge);
	lsdn_sbridge_add_stunnel(
		&pa->sbridge, &pa->sbridge_if, &pa->net->settings->vxlan.e2e_static.tunnel,
		&pa->stunnel_user, pa->net);
}

static void vxlan_static_destroy_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_sbridge_remove_stunnel(&pa->sbridge_if, &pa->stunnel_user);
	lsdn_sbridge_free(&pa->sbridge);
}

static void vxlan_static_add_virt(struct lsdn_virt *virt)
{
	lsdn_sbridge_add_virt(&virt->connected_through->sbridge, virt);
}

static void vxlan_static_remove_virt(struct lsdn_virt *virt)
{
	lsdn_sbridge_remove_virt(virt);
}

static void set_vxlan_metadata(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_remote_pa *pa = user;
	lsdn_action_set_tunnel_key(f, order,
		pa->local->net->vnet_id,
		pa->local->phys->attr_ip,
		pa->remote->phys->attr_ip);

}

static void vxlan_static_add_remote_pa (struct lsdn_remote_pa *pa)
{
	struct lsdn_action_desc *bra = &pa->sbridge_route.tunnel_action;
	bra->actions_count = 1;
	bra->fn = set_vxlan_metadata;
	bra->user = pa;
	lsdn_sbridge_add_route(&pa->local->sbridge_if, &pa->sbridge_route);
}

static void vxlan_static_remove_remote_pa (struct lsdn_remote_pa *pa)
{
	lsdn_sbridge_remove_route(&pa->sbridge_route);
}

static void vxlan_static_add_remote_virt(struct lsdn_remote_virt *virt)
{
	lsdn_sbridge_add_mac(&virt->pa->sbridge_route, &virt->sbridge_mac, *virt->virt->attr_mac);
}

static void vxlan_static_remove_remote_virt(struct lsdn_remote_virt *virt)
{
	lsdn_sbridge_remove_mac(&virt->sbridge_mac);
}

struct lsdn_net_ops lsdn_net_vxlan_static_ops = {
	.create_pa = vxlan_static_create_pa,
	.destroy_pa = vxlan_static_destroy_pa,
	.add_virt = vxlan_static_add_virt,
	.remove_virt = vxlan_static_remove_virt,
	.add_remote_pa = vxlan_static_add_remote_pa,
	.remove_remote_pa = vxlan_static_remove_remote_pa,
	.add_remote_virt = vxlan_static_add_remote_virt,
	.remove_remote_virt = vxlan_static_remove_remote_virt
};

struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	s->ctx = ctx;
	s->switch_type = LSDN_STATIC_E2E;
	s->nettype = LSDN_NET_VXLAN;
	s->ops = &lsdn_net_vxlan_static_ops;
	s->vxlan.port = port;
	s->vxlan.e2e_static.tunnel.refcount = 0;
	s->state = LSDN_STATE_NEW;
	return s;
}
