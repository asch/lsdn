#include "private/net.h"
#include "private/sbridge.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "private/errors.h"
#include "include/lsdn.h"
#include "include/nettypes.h"
#include "include/errors.h"
#include <stdarg.h>

static void vxlan_mcast_create_pa(struct lsdn_phys_attachment *a)
{
	struct lsdn_settings *s = a->net->settings;
	lsdn_err_t err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		&s->vxlan.mcast.mcast_ip,
		a->net->vnet_id,
		s->vxlan.port,
		true,
		false);
	if (err != LSDNE_OK)
		abort();

	lsdn_lbridge_init(a->net->ctx ,&a->lbridge);
	lsdn_lbridge_add(&a->lbridge, &a->lbridge_if, &a->tunnel_if);
}

static void vxlan_mcast_destroy_pa(struct lsdn_phys_attachment *a)
{
	lsdn_lbridge_remove(&a->lbridge_if);
	lsdn_lbridge_free(&a->lbridge);
	if(!a->net->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_link_delete(a->net->ctx->nlsock, &a->tunnel_if);
		if (err != LSDNE_OK)
			abort();
	}
	lsdn_if_free(&a->tunnel_if);
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.create_pa = vxlan_mcast_create_pa,
	.destroy_pa = vxlan_mcast_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt
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

	lsdn_settings_init_common(s, ctx);
	s->ops = &lsdn_net_vxlan_mcast_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = LSDN_LEARNING;
	s->vxlan.mcast.mcast_ip = mcast_ip;
	s->vxlan.port = port;
	return s;
}

static void vxlan_e2e_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port,
		true,
		false);
	if (err != LSDNE_OK)
		abort();

	lsdn_lbridge_init(a->net->ctx ,&a->lbridge);
	lsdn_lbridge_add(&a->lbridge, &a->lbridge_if, &a->tunnel_if);
}

static void vxlan_e2e_destroy_pa(struct lsdn_phys_attachment *a)
{
	lsdn_lbridge_remove(&a->lbridge_if);
	lsdn_lbridge_free(&a->lbridge);
	if(!a->net->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_link_delete(a->net->ctx->nlsock, &a->tunnel_if);
		if (err != LSDNE_OK)
			abort();
	}
	lsdn_if_free(&a->tunnel_if);
}

static void vxlan_e2e_add_remote_pa(struct lsdn_remote_pa *remote)
{
	/* Redirect broadcast packets to all remote PAs */
	struct lsdn_phys_attachment *local = remote->local;
	lsdn_err_t err = lsdn_fdb_add_entry(
		local->net->ctx->nlsock, local->tunnel_if.ifindex,
		lsdn_all_zeroes_mac,
		*remote->remote->phys->attr_ip);
	if (err != LSDNE_OK)
		abort();
}

static void vxlan_e2e_remove_remote_pa(struct lsdn_remote_pa *remote)
{
	if(remote->local->net->ctx->disable_decommit)
		return;

	struct lsdn_phys_attachment *local = remote->local;
	lsdn_err_t err = lsdn_fdb_remove_entry(
		local->net->ctx->nlsock, local->tunnel_if.ifindex,
		lsdn_all_zeroes_mac,
		*remote->remote->phys->attr_ip);
	if (err != LSDNE_OK)
		abort();
}

struct lsdn_net_ops lsdn_net_vxlan_e2e_ops = {
	.create_pa = vxlan_e2e_create_pa,
	.destroy_pa = vxlan_e2e_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.add_remote_pa = vxlan_e2e_add_remote_pa,
	.remove_remote_pa = vxlan_e2e_remove_remote_pa
};


struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_settings_init_common(s, ctx);
	s->ops = &lsdn_net_vxlan_e2e_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = LSDN_LEARNING_E2E;
	s->vxlan.port = port;
	return s;
}

/* Make sure the VXLAN interface operating in metadata mode for that UDP port exists. */
static void vxlan_use_stunnel(struct lsdn_settings *s)
{
	lsdn_err_t err;
	struct lsdn_context *ctx = s->ctx;
	struct lsdn_if *tunnel = &s->vxlan.e2e_static.tunnel;
	if (s->vxlan.e2e_static.refcount++ == 0) {
		err = lsdn_link_vxlan_create(
			ctx->nlsock,
			tunnel,
			NULL,
			lsdn_mk_ifname(ctx),
			NULL,
			0,
			s->vxlan.port,
			false,
			true);
		if (err != LSDNE_OK)
			abort();

		err = lsdn_link_set(ctx->nlsock, tunnel->ifindex, true);
		if (err != LSDNE_OK)
			abort();

		lsdn_sbridge_phys_if_init(ctx, &s->vxlan.e2e_static.tunnel_sbridge, tunnel, true);
	}
}

static void vxlan_release_stunnel(struct lsdn_settings *s)
{
	if (--s->vxlan.e2e_static.refcount == 0) {
		lsdn_sbridge_phys_if_free(&s->vxlan.e2e_static.tunnel_sbridge);
		if(!s->ctx->disable_decommit) {
			lsdn_err_t err = lsdn_link_delete(s->ctx->nlsock, &s->vxlan.e2e_static.tunnel);
			if (err != LSDNE_OK)
				abort();
		}
		lsdn_if_free(&s->vxlan.e2e_static.tunnel);
	}
}

static void vxlan_static_create_pa(struct lsdn_phys_attachment *pa)
{
	vxlan_use_stunnel(pa->net->settings);
	lsdn_sbridge_init(pa->net->ctx, &pa->sbridge);
	lsdn_sbridge_add_stunnel(
		&pa->sbridge, &pa->sbridge_if,
		&pa->net->settings->vxlan.e2e_static.tunnel_sbridge, pa->net);
}

static void vxlan_static_destroy_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_sbridge_remove_stunnel(&pa->sbridge_if);
	lsdn_sbridge_free(&pa->sbridge);
	vxlan_release_stunnel(pa->net->settings);
}

static void vxlan_static_add_virt(struct lsdn_virt *virt)
{
	lsdn_sbridge_add_virt(&virt->committed_to->sbridge, virt);
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

static void vxlan_static_validate_virt(struct lsdn_virt *virt)
{
	if (!virt->attr_mac)
		lsdn_problem_report(
			virt->network->ctx, LSDNP_VIRT_NOATTR,
			LSDNS_ATTR, "mac",
			LSDNS_VIRT, virt,
			LSDNS_NET, virt->network,
			LSDNS_END);
}

struct lsdn_net_ops lsdn_net_vxlan_static_ops = {
	.create_pa = vxlan_static_create_pa,
	.destroy_pa = vxlan_static_destroy_pa,
	.add_virt = vxlan_static_add_virt,
	.remove_virt = vxlan_static_remove_virt,
	.add_remote_pa = vxlan_static_add_remote_pa,
	.remove_remote_pa = vxlan_static_remove_remote_pa,
	.add_remote_virt = vxlan_static_add_remote_virt,
	.remove_remote_virt = vxlan_static_remove_remote_virt,
	.validate_virt = vxlan_static_validate_virt
};

struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_settings_init_common(s, ctx);
	s->switch_type = LSDN_STATIC_E2E;
	s->nettype = LSDN_NET_VXLAN;
	s->ops = &lsdn_net_vxlan_static_ops;
	s->vxlan.port = port;
	s->vxlan.e2e_static.refcount = 0;
	return s;
}
