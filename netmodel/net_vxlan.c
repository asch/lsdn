/** \file
 * Implementation of VXLAN network type. */
#include "private/net.h"
#include "private/sbridge.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "private/errors.h"
#include "include/lsdn.h"
#include "include/nettypes.h"
#include "include/errors.h"
#include <stdarg.h>

#define UDP_HEADER_LEN 8
#define VXLAN_HEADER_LEN 8
#define ETHERNET_FRAME_LEN 14
#define IPv4_HEADER_LEN 20
#define IPv6_HEADER_LEN 40

/** Calculate tunneling overhead for VXLAN networks.
 * Basis for VXLAN variants of `lsdn_net_ops.compute_tunneling_overhead`.
 *
 * Returns different results based on whether the network is IPv4 or IPv6,
 * because of different IP header sizes. */
static unsigned int vxlan_tunneling_overhead(enum lsdn_ipv ipv)
{
	if (ipv == LSDN_IPv4)
		return ETHERNET_FRAME_LEN + IPv4_HEADER_LEN + UDP_HEADER_LEN + VXLAN_HEADER_LEN;
	else
		return ETHERNET_FRAME_LEN + IPv6_HEADER_LEN + UDP_HEADER_LEN + VXLAN_HEADER_LEN;
}

static uint16_t vxlan_get_port(struct lsdn_settings *s)
{
	return s->vxlan.port;
}

static lsdn_ip_t vxlan_mcast_get_ip(struct lsdn_settings *s)
{
	return s->vxlan.mcast.mcast_ip;
}

/** \name Multicast VXLAN network.
 * This should describe what is mcast TODO. */
/** @{ */

/** Add a machine to VXLAN-multicast network.
 * Implements `lsdn_net_ops.create_pa`.
 *
 * Creates a multicast VXLAN link and connects it to a local Linux Bridge. */
static lsdn_err_t vxlan_mcast_create_pa(struct lsdn_phys_attachment *a)
{
	struct lsdn_settings *s = a->net->settings;
	lsdn_if_init(&a->tunnel_if);
	lsdn_err_t err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_iface_name(a->net->ctx),
		&s->vxlan.mcast.mcast_ip,
		a->net->vnet_id,
		s->vxlan.port,
		true,
		false,
		s->vxlan.mcast.mcast_ip.v);
	if (err != LSDNE_OK) {
		lsdn_if_free(&a->tunnel_if);
		return err;
	}

	err = lsdn_lbridge_create_pa(a);
	if (err != LSDNE_OK) {
		if (lsdn_link_delete(a->net->ctx->nlsock, &a->tunnel_if) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
		lsdn_if_free(&a->tunnel_if);
		return err;
	}
	return LSDNE_OK;
}

/** Calculate tunneling overhead for VXLAN-multicast network.
 * Implements `lsdn_net_ops.compute_tunneling_overhead`. */
static unsigned int vxlan_mcast_tunneling_overhead(struct lsdn_phys_attachment *pa)
{
	struct lsdn_settings *s = pa->net->settings;
	enum lsdn_ipv ipv = s->vxlan.mcast.mcast_ip.v;
	return vxlan_tunneling_overhead(ipv);
}

/** Callbacks for VXLAN-multicast network.
 * Reuses network functions from `lbridge.c`. */
struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.type = "vxlan/mcast",
	.get_port = vxlan_get_port,
	.get_ip = vxlan_mcast_get_ip,
	.create_pa = vxlan_mcast_create_pa,
	.destroy_pa = lsdn_lbridge_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.compute_tunneling_overhead = vxlan_mcast_tunneling_overhead
};

/** Create settings for a new VXLAN-multicast network.
 * @return new `lsdn_settings` instance. The caller is responsible for freeing it. */
struct lsdn_settings *lsdn_settings_new_vxlan_mcast(
	struct lsdn_context *ctx,
	lsdn_ip_t mcast_ip, uint16_t port)
{
	if(port == 0)
		port = 4789;

	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_err_t err = lsdn_settings_init_common(s, ctx);
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(s);
		ret_ptr(ctx, NULL);
	}
	s->ops = &lsdn_net_vxlan_mcast_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = LSDN_LEARNING;
	s->vxlan.mcast.mcast_ip = mcast_ip;
	s->vxlan.port = port;
	return s;
}

/** @} */

/** \name End-to-End VXLAN network.
 * TODO this should explain e2e */
/** @{ */

/** Add a local machine to VXLAN-e2e network.
 * Implements `lsdn_net_ops.create_pa`.
 *
 * Sets up a VXLAN tunnel and connects it to a Linux Bridge. */
static lsdn_err_t vxlan_e2e_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_iface_name(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port,
		true,
		false,
		a->phys->attr_ip->v);
	if (err != LSDNE_OK)
		return err;

	err = lsdn_lbridge_create_pa(a);
	if (err != LSDNE_OK) {
		acc_inconsistent(&err, lsdn_link_delete(a->net->ctx->nlsock, &a->tunnel_if));
		lsdn_if_free(&a->tunnel_if);
	}
	return err;
}

/** Adds a remote machine to VXLAN-e2e network.
 * Implements `lsdn_net_ops.add_remote_pa`. 
 *
 * Sets up a static route to the given remote phys. */
static lsdn_err_t vxlan_e2e_add_remote_pa(struct lsdn_remote_pa *remote)
{
	/* Redirect broadcast packets to all remote PAs */
	struct lsdn_phys_attachment *local = remote->local;
	return lsdn_fdb_add_entry(
		local->net->ctx->nlsock, local->tunnel_if.ifindex,
		lsdn_all_zeroes_mac,
		*remote->remote->phys->attr_ip);
}

/** Remove a remote machine from VXLAN-e2e network.
 * Implements `lsdn_net_ops.remove_remote_pa`.
 *
 * Tears down a static route to the given remote phys. */
static lsdn_err_t vxlan_e2e_remove_remote_pa(struct lsdn_remote_pa *remote)
{
	if(remote->local->net->ctx->disable_decommit)
		return LSDNE_OK;

	struct lsdn_phys_attachment *local = remote->local;
	return lsdn_fdb_remove_entry(
		local->net->ctx->nlsock, local->tunnel_if.ifindex,
		lsdn_all_zeroes_mac,
		*remote->remote->phys->attr_ip);
}

/** Validate a phys attachment for use in VXLAN-e2e network.
 * Implements `lsdn_net_ops.validate_pa`.
 *
 * Checks that the remote IP address is filled out. */
static void vxlan_e2e_validate_pa(struct lsdn_phys_attachment *a)
{
	if (!a->phys->attr_ip)
		lsdn_problem_report(
			a->phys->ctx, LSDNP_PHYS_NOATTR,
			LSDNS_ATTR, "ip",
			LSDNS_PHYS, a->phys,
			LSDNS_NET, a->net,
			LSDNS_END);
}

/** Calculate tunneling overhead for VXLAN-e2e network.
 * Implements `lsdn_net_ops.compute_tunneling_overhead`. */
static unsigned int vxlan_e2e_tunneling_overhead(struct lsdn_phys_attachment *pa)
{
	enum lsdn_ipv ipv = pa->phys->attr_ip->v;
	return vxlan_tunneling_overhead(ipv);
}

/** Callbacks for VXLAN-e2e network.
 * Reuses network functions from `lbridge.c` for adding and removing virts
 * and for tearing down the network. Adding remote physes is implemented
 * specifically for VXLAN-e2e. */
struct lsdn_net_ops lsdn_net_vxlan_e2e_ops = {
	.type = "vxlan/e2e",
	.get_port = vxlan_get_port,
	.create_pa = vxlan_e2e_create_pa,
	.destroy_pa = lsdn_lbridge_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.add_remote_pa = vxlan_e2e_add_remote_pa,
	.remove_remote_pa = vxlan_e2e_remove_remote_pa,
	.validate_pa = vxlan_e2e_validate_pa,
	.compute_tunneling_overhead = vxlan_e2e_tunneling_overhead
};

/** Create settings for a new VXLAN-e2e network.
 * @return new `lsdn_settings` instance. The caller is responsible for freeing it. */
struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_err_t err = lsdn_settings_init_common(s, ctx);
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(s);
		ret_ptr(ctx, NULL);
	}
	s->ops = &lsdn_net_vxlan_e2e_ops;
	s->nettype = LSDN_NET_VXLAN;
	s->switch_type = LSDN_LEARNING_E2E;
	s->vxlan.port = port;
	return s;
}

/** @} */

/** \name VXLAN with static routes.
 * explain static routest TODO */
/** @{ */

/** Configure static tunnel for a phys.
 * Makes sure the VXLAN interface for a given UDP port exists
 * and is operating in metadata mode. Increases refcount for the
 * static tunnel. */
static lsdn_err_t vxlan_use_stunnel(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err = LSDNE_OK;
	struct lsdn_settings *s = a->net->settings;
	struct lsdn_context *ctx = s->ctx;
	struct lsdn_if *tunnel = &s->vxlan.e2e_static.tunnel;
	struct lsdn_ruleset *rules_in = &s->vxlan.e2e_static.ruleset_in;
	if (s->vxlan.e2e_static.refcount++ == 0) {
		err = lsdn_link_vxlan_create(
			ctx->nlsock,
			tunnel,
			NULL,
			lsdn_mk_iface_name(ctx),
			NULL,
			0,
			s->vxlan.port,
			false,
			true,
			a->phys->attr_ip->v);
		if (err != LSDNE_OK)
			goto cleanup_link;

		err = lsdn_link_set(ctx->nlsock, tunnel->ifindex, true);
		if (err != LSDNE_OK)
			goto cleanup_link;

		err = lsdn_prepare_rulesets(ctx, tunnel, rules_in, NULL);
		if (err != LSDNE_OK)
			goto cleanup_link;

		err = lsdn_sbridge_phys_if_init(
			ctx, &s->vxlan.e2e_static.tunnel_sbridge, tunnel, true, rules_in);

		if (err != LSDNE_OK)
			goto cleanup_phys_if;
	}
	return err;

	cleanup_phys_if:
	acc_inconsistent(&err, lsdn_sbridge_phys_if_free(&s->vxlan.e2e_static.tunnel_sbridge));
	cleanup_link:
	acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, tunnel));
	lsdn_if_free(tunnel);
	return err;
}

/** Release a static tunnel.
 * Decreases refcount for the static tunnel. If it drops to zero, tears
 * down the tunnel. */
static void vxlan_release_stunnel(struct lsdn_settings *s)
{
	// TODO: add error handling
	if (--s->vxlan.e2e_static.refcount == 0) {
		lsdn_sbridge_phys_if_free(&s->vxlan.e2e_static.tunnel_sbridge);
		if(!s->ctx->disable_decommit) {
			lsdn_err_t err = lsdn_link_delete(s->ctx->nlsock, &s->vxlan.e2e_static.tunnel);
			if (err != LSDNE_OK)
				abort();
		}
		lsdn_ruleset_free(&s->vxlan.e2e_static.ruleset_in);
		lsdn_if_free(&s->vxlan.e2e_static.tunnel);
	}
}

/** Add a local machine to VXLAN-static network.
 * Implements `lsdn_net_ops.create_pa`.
 *
 * Ensures local static tunnel exists and bridges the phys into it. */
static lsdn_err_t vxlan_static_create_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_err_t err = LSDNE_OK;
	err = vxlan_use_stunnel(pa);
	if (err != LSDNE_OK)
		goto no_cleanup;
	err = lsdn_sbridge_init(pa->net->ctx, &pa->sbridge);
	if (err != LSDNE_OK)
		goto cleanup_tunnel;
	err = lsdn_sbridge_add_stunnel(
		&pa->sbridge, &pa->sbridge_if,
		&pa->net->settings->vxlan.e2e_static.tunnel_sbridge, pa->net);
	if (err != LSDNE_OK)
		goto cleanup_sbridge;
	return err;

	cleanup_sbridge:
	acc_inconsistent(&err, lsdn_sbridge_free(&pa->sbridge));
	cleanup_tunnel:
	vxlan_release_stunnel(pa->net->settings);
	no_cleanup:
	return err;
}

/** Remove a local machine from VXLAN-static network.
 * Implements `lsdn_net_ops.destroy_pa`.
 *
 * Tears down the local static bridge and releases the tunnel. */
static lsdn_err_t vxlan_static_destroy_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_err_t err = LSDNE_OK;
	acc_inconsistent(&err, lsdn_sbridge_remove_stunnel(&pa->sbridge_if));
	acc_inconsistent(&err, lsdn_sbridge_free(&pa->sbridge));
	vxlan_release_stunnel(pa->net->settings);
	return err;
}

/** Add a local virt to VXLAN-static network.
 * Implements `lsdn_net_ops.add_virt`.
 *
 * Connects the virt to the static tunnel bridge. */
static lsdn_err_t vxlan_static_add_virt(struct lsdn_virt *virt)
{
	return lsdn_sbridge_add_virt(&virt->committed_to->sbridge, virt);
}

/** Remove a local virt from VXLAN-static network.
 * Implements `lsdn_net_ops.remove_virt`.
 *
 * Disconnects the virt from the static tunnel bridge. */
static lsdn_err_t vxlan_static_remove_virt(struct lsdn_virt *virt)
{
	return lsdn_sbridge_remove_virt(virt);
}

/** TODO */
static void set_vxlan_metadata(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_remote_pa *pa = user;
	lsdn_action_set_tunnel_key(f, order,
		pa->local->net->vnet_id,
		pa->local->phys->attr_ip,
		pa->remote->phys->attr_ip);

}

/** Add a remote phys to VXLAN-static network.
 * Implements `lsdn_net_ops.add_remote_pa`.
 *
 * Inserts an appropriate route into the static bridge. */
static lsdn_err_t vxlan_static_add_remote_pa (struct lsdn_remote_pa *pa)
{
	struct lsdn_action_desc *bra = &pa->sbridge_route.tunnel_action;
	bra->actions_count = 1;
	bra->fn = set_vxlan_metadata;
	bra->user = pa;
	return lsdn_sbridge_add_route(&pa->local->sbridge_if, &pa->sbridge_route);
}

/** Remove a remote phys from VXLAN-static network.
 * Implements `lsdn_net_ops.remove_remote_pa`.
 *
 * Removes the appropriate route from the static bridge. */
static lsdn_err_t vxlan_static_remove_remote_pa (struct lsdn_remote_pa *pa)
{
	return lsdn_sbridge_remove_route(&pa->sbridge_route);
}

/** Add a remote virt to VXLAN-static network.
 * Implements `lsdn_net_ops.add_remote_virt`.
 *
 * Adds a MAC-based rule into the static bridge. */
static lsdn_err_t vxlan_static_add_remote_virt(struct lsdn_remote_virt *virt)
{
	return lsdn_sbridge_add_mac(&virt->pa->sbridge_route, &virt->sbridge_mac, *virt->virt->attr_mac);
}

/** Remove a remote virt from VXLAN-static network.
 * Implemens `lsdn_net_ops.remove_remote_virt`.
 *
 * Drops the appropriate rule from the static bridge. */
static lsdn_err_t vxlan_static_remove_remote_virt(struct lsdn_remote_virt *virt)
{
	return lsdn_sbridge_remove_mac(&virt->sbridge_mac);
}

/** Validate a phys attachment for use in VXLAN-static network.
 * Implements `lsdn_net_ops.validate_pa`.
 *
 * Checks that the remote IP address is filled out. */
static void vxlan_static_validate_pa(struct lsdn_phys_attachment *a)
{
	if (!a->phys->attr_ip)
		lsdn_problem_report(
			a->phys->ctx, LSDNP_PHYS_NOATTR,
			LSDNS_ATTR, "ip",
			LSDNS_PHYS, a->phys,
			LSDNS_NET, a->net,
			LSDNS_END);
}

/** Validate a virt for use in VXLAN-static network.
 * Implements `lsdn_net_ops.validate_virt`.
 *
 * Checks that the MAC address of the virt is filled out. */
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

/** Calculate tunneling overhead for VXLAN-static network.
 * Implements `lsdn_net_ops.compute_tunneling_overhead`. */
static unsigned int vxlan_static_tunneling_overhead(struct lsdn_phys_attachment *pa)
{
	enum lsdn_ipv ipv = pa->phys->attr_ip->v;
	return vxlan_tunneling_overhead(ipv);
}

/** Callbacks for VXLAN-static network. */
struct lsdn_net_ops lsdn_net_vxlan_static_ops = {
	.type = "vxlan/static",
	.get_port = vxlan_get_port,
	.create_pa = vxlan_static_create_pa,
	.destroy_pa = vxlan_static_destroy_pa,
	.add_virt = vxlan_static_add_virt,
	.remove_virt = vxlan_static_remove_virt,
	.add_remote_pa = vxlan_static_add_remote_pa,
	.remove_remote_pa = vxlan_static_remove_remote_pa,
	.add_remote_virt = vxlan_static_add_remote_virt,
	.remove_remote_virt = vxlan_static_remove_remote_virt,
	.validate_pa = vxlan_static_validate_pa,
	.validate_virt = vxlan_static_validate_virt,
	.compute_tunneling_overhead = vxlan_static_tunneling_overhead
};

/** Create settings for a new VXLAN-static network.
 * @return new `lsdn_settings` instance. The caller is responsible for freeing it. */
struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_err_t err = lsdn_settings_init_common(s, ctx);
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(s);
		ret_ptr(ctx, NULL);
	}
	s->switch_type = LSDN_STATIC_E2E;
	s->nettype = LSDN_NET_VXLAN;
	s->ops = &lsdn_net_vxlan_static_ops;
	s->vxlan.port = port;
	s->vxlan.e2e_static.refcount = 0;
	return s;
}
