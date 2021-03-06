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

	lsdn_err_t err = lsdn_link_bridge_create(
		ctx->nlsock, &bridge_if, lsdn_mk_iface_name(ctx), ctx->overwrite);
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

/** Free the #lsdn_lbridge structure. */
lsdn_err_t lsdn_lbridge_free(struct lsdn_lbridge *br)
{
	if (!br->ctx->disable_decommit)
		if (lsdn_link_delete(br->ctx->nlsock, &br->bridge_if) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
	lsdn_if_free(&br->bridge_if);

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
 * These functions are used as callbacks in #lsdn_net_ops (or by callbacks, in case of
 * #lsdn_lbridge_create_pa) for network types that use the Linux bridge functionality. */
/** @{ */

/** Create a local bridge and connect the given phys to it.
 * Not used as a callback directly, but called from
 * implementations of #lsdn_net_ops.create_pa callbacks. */
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
 * Implements #lsdn_net_ops.destroy_pa.
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
 * Implements #lsdn_net_ops.add_virt. */
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
 * Implements #lsdn_net_ops.remove_virt. */
lsdn_err_t lsdn_lbridge_remove_virt(struct lsdn_virt *v)
{
	lsdn_err_t err = LSDNE_OK;
	lsdn_ruleset_free(&v->rules_in);
	lsdn_ruleset_free(&v->rules_out);
	acc_inconsistent(&err, lsdn_lbridge_remove(&v->lbridge_if));
	acc_inconsistent(&err, lsdn_cleanup_rulesets(v->network->ctx, &v->committed_if, &v->rules_in, &v->rules_out));
	return err;
}

#define LBRIDGE_FILTER 1
#define LBRIDGE_REDIR 2
#define INGRESS_MARK 1

static void encap_redir_mkaction(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_lbridge_route *route = user;
	if (route->tunnel_action.fn) {
		route->tunnel_action.fn(f, order, route->tunnel_action.user);
		order += route->tunnel_action.actions_count;
	}
	lsdn_action_redir_egress_add(f, order, route->phys_if->iface->ifindex);
}


static void ingress_redir_mkaction(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_lbridge_route *route = user;
	lsdn_action_skbmark(f, order++, INGRESS_MARK);
	lsdn_action_redir_ingress_add(f, order++, route->dummy_if.ifindex);
}

lsdn_err_t lsdn_lbridge_add_route(struct lsdn_lbridge *br, struct lsdn_lbridge_route *route, uint32_t vid)
{
	struct lsdn_context *ctx = br->ctx;
	lsdn_err_t err = LSDNE_OK;
	err = lsdn_link_dummy_create(ctx->nlsock, &route->dummy_if, lsdn_mk_iface_name(ctx), false);
	if (err != LSDNE_OK)
		return err;
	/* Prepare outgoing rules to forward the packet with appropriate encapsulation */
	err = lsdn_prepare_rulesets(ctx, &route->dummy_if, NULL, &route->ruleset);
	if (err != LSDNE_OK)
		goto cleanup_if;

	if (!(route->out_redir = lsdn_ruleset_define_prio(&route->ruleset, LBRIDGE_REDIR))) {
		err = LSDNE_NOMEM;
		goto cleanup_ruleset;
	}

	/* Filter rule */
	struct lsdn_filter *f = lsdn_filter_fw_init(
		route->ruleset.iface->ifindex, 1,
		route->ruleset.parent_handle, route->ruleset.chain, route->ruleset.prio_start + LBRIDGE_FILTER);
	if (!f)
		abort();
	lsdn_fw_actions_start(f);
	lsdn_action_drop(f, INGRESS_MARK);
	lsdn_fw_actions_end(f);
	err = lsdn_filter_create(ctx->nlsock, f);
	lsdn_filter_free(f);
	if (err != LSDNE_OK) {
		goto cleanup_out_redir;
	}


	/* Redir rule */
	struct lsdn_rule *r = &route->out_redir_rule;
	r->subprio = 0;
	lsdn_action_init(&r->action, route->tunnel_action.actions_count + 1,
			 encap_redir_mkaction, route);
	err = lsdn_ruleset_add(route->out_redir, r);
	if (err != LSDNE_OK)
		goto cleanup_out_redir;

	/* And a reverse forwarding on the tunnel for our VID. */
	assert(route->phys_if->rules_source->targets[1] == route->vid_match);
	assert(route->phys_if->rules_source->targets[0] == route->source_match);

	struct lsdn_rule* in_rule = &route->in_redir_rule;
	in_rule->subprio = LSDN_SBRIDGE_IF_SUBPRIO;
	in_rule->matches[0] = route->source_matchdata;
	in_rule->matches[1] = route->vid_matchdata;
	lsdn_action_init(&in_rule->action, 2, ingress_redir_mkaction, route);
	err = lsdn_ruleset_add(route->phys_if->rules_source, in_rule);
	if (err != LSDNE_OK)
		goto cleanup_out_redir_rule;

	err = lsdn_lbridge_add(br, &route->lbridge_if, &route->dummy_if);
	if (err != LSDNE_OK)
		goto cleanup_tunnel_ruleset;
	err = lsdn_link_set(ctx->nlsock, route->dummy_if.ifindex, true);
	if (err != LSDNE_OK)
		goto cleanup_lbridge;

	return err;

	cleanup_lbridge:
	acc_inconsistent(&err, lsdn_lbridge_remove(&route->lbridge_if));
	cleanup_tunnel_ruleset:
	acc_inconsistent(&err, lsdn_ruleset_remove(in_rule));
	cleanup_out_redir_rule:
	acc_inconsistent(&err, lsdn_ruleset_remove(&route->out_redir_rule));
	cleanup_out_redir:
	lsdn_ruleset_remove_prio(route->out_redir);
	cleanup_ruleset:
	acc_inconsistent(&err, lsdn_cleanup_rulesets(ctx, &route->dummy_if, NULL, &route->ruleset));
	cleanup_if:
	acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, &route->dummy_if));
	return err;
}

lsdn_err_t lsdn_lbridge_remove_route(struct lsdn_lbridge_route *route)
{
	struct lsdn_context *ctx = route->ruleset.ctx;

	lsdn_err_t err = LSDNE_OK;
	acc_inconsistent(&err, lsdn_lbridge_remove(&route->lbridge_if));
	acc_inconsistent(&err, lsdn_ruleset_remove(&route->in_redir_rule));
	acc_inconsistent(&err, lsdn_ruleset_remove(&route->out_redir_rule));
	lsdn_ruleset_remove_prio(route->out_redir);
	acc_inconsistent(&err, lsdn_cleanup_rulesets(ctx, &route->dummy_if, NULL, &route->ruleset));
	if (!ctx->disable_decommit)
		acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, &route->dummy_if));
	lsdn_if_free(&route->dummy_if);
	return err;
}

/** @} */
