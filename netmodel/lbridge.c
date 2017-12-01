/** \file
 * Linux Bridge management functions */
#include "private/lbridge.h"
#include "private/net.h"
#include "include/lsdn.h"

/** Set up a Linux Bridge and associate it with a context. */
void lsdn_lbridge_init(struct lsdn_context *ctx, struct lsdn_lbridge *br)
{
	struct lsdn_if bridge_if;
	lsdn_if_init(&bridge_if);

	lsdn_err_t err = lsdn_link_bridge_create(ctx->nlsock, &bridge_if, lsdn_mk_ifname(ctx));
	if(err != LSDNE_OK)
		abort();

	err = lsdn_link_set(ctx->nlsock, bridge_if.ifindex, true);
	if(err != LSDNE_OK)
		abort();

	br->ctx = ctx;
	br->bridge_if = bridge_if;
}

/** Free the `lsdn_lbridge` structure. */
void lsdn_lbridge_free(struct lsdn_lbridge *br)
{
	if (!br->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_link_delete(br->ctx->nlsock, &br->bridge_if);
		if (err != LSDNE_OK)
			abort();
	}
	lsdn_if_free(&br->bridge_if);
}

/** Add an interface to the bridge.
 * @param br Bridge.
 * @param br_if Resulting bridge interface structure.
 * @param iface Interface to connect. */
void lsdn_lbridge_add(struct lsdn_lbridge *br, struct lsdn_lbridge_if *br_if, struct lsdn_if *iface)
{
	lsdn_err_t err = lsdn_link_set_master(br->ctx->nlsock, br->bridge_if.ifindex, iface->ifindex);
	if(err != LSDNE_OK)
		abort();

	err = lsdn_link_set(br->ctx->nlsock, iface->ifindex, true);
	if(err != LSDNE_OK)
		abort();

	br_if->br = br;
	br_if->iface = iface;
}

/** Remove an interface from the bridge. */
void lsdn_lbridge_remove(struct lsdn_lbridge_if *iface)
{
	if (!iface->br->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_link_set_master(iface->br->ctx->nlsock, 0, iface->iface->ifindex);
		if (err != LSDNE_OK)
			abort();
	}
}

/** \name Network operations.
 * These functions are used as callbacks in `lsdn_net_ops` (or by callbacks, in case of
 * `lsdn_lbridge_create_pa`) for network types that use the Linux bridge functionality. */
/** @{ */

/** Create a local bridge and connect the given phys to it.
 * Not used as a callback directly, but called from
 * implementations of `lsdn_net_ops.create_pa` callbacks. */
void lsdn_lbridge_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_lbridge_init(a->net->ctx, &a->lbridge);
	lsdn_lbridge_add(&a->lbridge, &a->lbridge_if, &a->tunnel_if);
}

/** Destroy a local bridge.
 * Implements `lsdn_net_ops.destroy_pa`.
 *
 * Removes the Linux Bridge interface and frees the PA's tunnel interface.
 * Potentially also removes PA's TC rules. */
void lsdn_lbridge_destroy_pa(struct lsdn_phys_attachment *a)
{
	lsdn_lbridge_remove(&a->lbridge_if);
	lsdn_lbridge_free(&a->lbridge);
	if(!a->net->ctx->disable_decommit) {
		int err = lsdn_link_delete(a->net->ctx->nlsock, &a->tunnel_if);
		if (err)
			abort();
	}
	lsdn_if_free(&a->tunnel_if);
}

/** Connect a local virt to the Linux Bridge.
 * Implements `lsdn_net_ops.add_virt`. */
void lsdn_lbridge_add_virt(struct lsdn_virt *v)
{
	struct lsdn_phys_attachment *a = v->connected_through;
	lsdn_lbridge_add(&a->lbridge, &v->lbridge_if, &v->committed_if);
	lsdn_prepare_rulesets(v->network->ctx, &v->committed_if, &v->rules_in, &v->rules_out);
}

/** Disconnect a local virt from the Linux Bridge.
 * Implements `lsdn_net_ops.remove_virt`. */
void lsdn_lbridge_remove_virt(struct lsdn_virt *v)
{
	lsdn_ruleset_free(&v->rules_in);
	lsdn_ruleset_free(&v->rules_out);
	lsdn_lbridge_remove(&v->lbridge_if);
}

/** @} */
