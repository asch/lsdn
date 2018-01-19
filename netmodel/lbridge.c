/** \file
 * Linux Bridge management functions */
#include "private/lbridge.h"
#include "private/net.h"
#include "private/errors.h"
#include "include/lsdn.h"

/** Set up a Linux Bridge and associate it with a context. */
lsdn_err_t lsdn_lbridge_init(struct lsdn_context *ctx, struct lsdn_lbridge *br)
{
	struct lsdn_if bridge_if;
	lsdn_if_init(&bridge_if);

	lsdn_err_t err = lsdn_link_bridge_create(ctx->nlsock, &bridge_if, lsdn_mk_iface_name(ctx));
	if(err != LSDNE_OK) {
		lsdn_if_free(&bridge_if);
		return err;
	}

	err = lsdn_link_set(ctx->nlsock, bridge_if.ifindex, true);
	if(err != LSDNE_OK) {
		lsdn_if_free(&bridge_if);
		if (lsdn_link_delete(ctx->nlsock, &bridge_if) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
		return err;
	}

	br->ctx = ctx;
	br->bridge_if = bridge_if;
	return LSDNE_OK;
}

/** Free the `lsdn_lbridge` structure. */
lsdn_err_t lsdn_lbridge_free(struct lsdn_lbridge *br)
{
	if (!br->ctx->disable_decommit)
		if (lsdn_link_delete(br->ctx->nlsock, &br->bridge_if) != LSDNE_OK)
			return LSDNE_INCONSISTENT;

	return LSDNE_OK;
}

/** Add an interface to the bridge.
 * @param br Bridge.
 * @param br_if Resulting bridge interface structure.
 * @param iface Interface to connect. */
lsdn_err_t lsdn_lbridge_add(struct lsdn_lbridge *br, struct lsdn_lbridge_if *br_if, struct lsdn_if *iface)
{
	lsdn_err_t err = lsdn_link_set_master(br->ctx->nlsock, br->bridge_if.ifindex, iface->ifindex);
	if(err != LSDNE_OK)
		return err;

	err = lsdn_link_set(br->ctx->nlsock, iface->ifindex, true);
	if(err != LSDNE_OK) {
		if (lsdn_link_set_master(br->ctx->nlsock, 0, iface->ifindex) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
		return err;
	}

	br_if->br = br;
	br_if->iface = iface;
	return LSDNE_OK;
}

/** Remove an interface from the bridge. */
lsdn_err_t lsdn_lbridge_remove(struct lsdn_lbridge_if *iface)
{
	if (!iface->br->ctx->disable_decommit) {
		if (lsdn_link_set_master(iface->br->ctx->nlsock, 0, iface->iface->ifindex) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
	}
	return LSDNE_OK;
}

/** \name Network operations.
 * These functions are used as callbacks in `lsdn_net_ops` (or by callbacks, in case of
 * `lsdn_lbridge_create_pa`) for network types that use the Linux bridge functionality. */
/** @{ */

/** Create a local bridge and connect the given phys to it.
 * Not used as a callback directly, but called from
 * implementations of `lsdn_net_ops.create_pa` callbacks. */
lsdn_err_t lsdn_lbridge_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err = lsdn_lbridge_init(a->net->ctx, &a->lbridge);
	if (err != LSDNE_OK)
		return err;
	err = lsdn_lbridge_add(&a->lbridge, &a->lbridge_if, &a->tunnel_if);
	if (err != LSDNE_OK) {
		if (lsdn_lbridge_free(&a->lbridge) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
		return err;
	}
	return LSDNE_OK;

}

/** Destroy a local bridge.
 * Implements `lsdn_net_ops.destroy_pa`.
 *
 * Removes the Linux Bridge interface and frees the PA's tunnel interface.
 * Potentially also removes PA's TC rules. */
lsdn_err_t lsdn_lbridge_destroy_pa(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err = LSDNE_OK;
	acc_inconsistent(&err, lsdn_lbridge_remove(&a->lbridge_if));
	acc_inconsistent(&err, lsdn_lbridge_free(&a->lbridge));

	if (!a->net->ctx->disable_decommit)
		acc_inconsistent(&err, lsdn_link_delete(a->net->ctx->nlsock, &a->tunnel_if));
	lsdn_if_free(&a->tunnel_if);
	return err;
}

/** Connect a local virt to the Linux Bridge.
 * Implements `lsdn_net_ops.add_virt`. */
lsdn_err_t lsdn_lbridge_add_virt(struct lsdn_virt *v)
{
	struct lsdn_phys_attachment *a = v->connected_through;
	lsdn_err_t err = lsdn_prepare_rulesets(v->network->ctx, &v->committed_if, &v->rules_in, &v->rules_out);
	if (err != LSDNE_OK)
		return err;

	err = lsdn_lbridge_add(&a->lbridge, &v->lbridge_if, &v->committed_if);
	if (err != LSDNE_OK) {
		if (lsdn_cleanup_rulesets(v->network->ctx, &v->committed_if, &v->rules_in, &v->rules_out) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
		return err;
	}

	return LSDNE_OK;
}

/** Disconnect a local virt from the Linux Bridge.
 * Implements `lsdn_net_ops.remove_virt`. */
lsdn_err_t lsdn_lbridge_remove_virt(struct lsdn_virt *v)
{
	lsdn_err_t err = LSDNE_OK;
	lsdn_ruleset_free(&v->rules_in);
	lsdn_ruleset_free(&v->rules_out);
	acc_inconsistent(&err, lsdn_lbridge_remove(&v->lbridge_if));
	acc_inconsistent(&err, lsdn_cleanup_rulesets(v->network->ctx, &v->committed_if, &v->rules_in, &v->rules_out));
	return err;
}

/** @} */
