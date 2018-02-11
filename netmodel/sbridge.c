#include "private/sbridge.h"
#include "private/net.h"
#include "private/errors.h"

enum {CL_OWNER, CL_DEST};

/* Broadcast rule (well, action) on the sbridge_if. */
struct if_br_action {
	struct lsdn_clist_entry clist;
	struct lsdn_sbridge_route *route;
	struct lsdn_broadcast_action action;
};

static lsdn_err_t if_br_action_free(void *user)
{
	struct if_br_action *action = user;
	lsdn_err_t err = lsdn_broadcast_remove(&action->action);
	free(action);
	return err;
}

static void if_br_mkaction(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct if_br_action *action = user;
	struct lsdn_action_desc *tun_action = &action->route->tunnel_action;
	if (tun_action->fn) {
		tun_action->fn(f, order, tun_action->user);
		order += tun_action->actions_count;
	}
	lsdn_action_mirror_egress_add(f, order, action->route->iface->phys_if->iface->ifindex);
}

static lsdn_err_t if_br_make(struct lsdn_sbridge_if *from, struct lsdn_sbridge_route *to)
{
	struct if_br_action *bra = malloc(sizeof(*bra));
	if (!bra)
		return LSDNE_NOMEM;
	lsdn_clist_init_entry(&bra->clist, if_br_action_free, bra);
	bra->route = to;
	struct lsdn_action_desc desc;
	/* Set tunnel metadata + mirred */
	desc.name = NULL;
	desc.actions_count = to->tunnel_action.actions_count + 1;
	desc.fn = if_br_mkaction;
	desc.user = bra;
	lsdn_err_t err = lsdn_broadcast_add(&from->broadcast, &bra->action, desc);
	if (err != LSDNE_OK) {
		free (bra);
		return err;
	}

	lsdn_clist_add(&to->cl_dest, &bra->clist);
	lsdn_clist_add(&from->cl_owner, &bra->clist);
	return err;
}

/* Routing rule on the dummy bridging interface */
struct br_forward_rule {
	struct lsdn_clist_entry clist;
	struct lsdn_sbridge_mac *mac;
	struct lsdn_rule rule;
};

static lsdn_err_t br_forward_rule_free(void *user)
{
	struct br_forward_rule *fwdr = user;
	lsdn_err_t err = lsdn_ruleset_remove(&fwdr->rule);
	free(fwdr);
	return err;
}

/** Action constructor callback for forwarding rule */
static void br_forward_mkaction(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct br_forward_rule *fwdr = user;
	struct lsdn_sbridge_mac *mac = fwdr->mac;
	if (mac->route->tunnel_action.fn) {
		mac->route->tunnel_action.fn(f, order, mac->route->tunnel_action.user);
		order += mac->route->tunnel_action.actions_count;
	}
	lsdn_action_redir_egress_add(f, order, mac->route->iface->phys_if->iface->ifindex);
}

/** Create a forwarding rule for a mac address */
static lsdn_err_t br_forward_make(struct lsdn_sbridge_mac *mac)
{
	lsdn_err_t err;
	struct lsdn_sbridge *br = mac->route->iface->bridge;
	struct br_forward_rule *fwdr = malloc(sizeof(*fwdr));
	if (!fwdr)
		return LSDNE_NOMEM;
	lsdn_clist_init_entry(&fwdr->clist, br_forward_rule_free, fwdr);	
	fwdr->mac = mac;
	lsdn_action_init(&fwdr->rule.action, 1 +  mac->route->tunnel_action.actions_count, br_forward_mkaction, fwdr);
	fwdr->rule.subprio = 0;
	fwdr->rule.matches[0].mac = mac->mac;
	err = lsdn_ruleset_add(br->bridge_ruleset, &fwdr->rule);
	if (err != LSDNE_OK) {
		free(fwdr);
		return err;
	}
	lsdn_clist_add(&mac->cl_dest, &fwdr->clist);
	return err;
}

lsdn_err_t lsdn_sbridge_init(struct lsdn_context *ctx, struct lsdn_sbridge *br)
{
	struct lsdn_if sbridge_if;
	struct lsdn_ruleset_prio *prio;
	lsdn_if_init(&sbridge_if);
	lsdn_err_t err = lsdn_link_dummy_create(ctx->nlsock, &sbridge_if, lsdn_mk_iface_name(ctx));
	if (err != LSDNE_OK)
		return err;

	err = lsdn_qdisc_ingress_create(ctx->nlsock, sbridge_if.ifindex);
	if (err != LSDNE_OK)
		goto cleanup_if;

	err = lsdn_link_set(ctx->nlsock, sbridge_if.ifindex, true);
	if (err != LSDNE_OK)
		goto cleanup_if;

	br->bridge_if = sbridge_if;
	br->ctx = ctx;
	lsdn_ruleset_init(
		&br->bridge_ruleset_main, ctx, &br->bridge_if,
		LSDN_INGRESS_HANDLE, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY, 1);
	prio = br->bridge_ruleset =
			lsdn_ruleset_define_prio(&br->bridge_ruleset_main, 0);
	if (!prio)
		goto cleanup_ruleset;

	prio->targets[0] = LSDN_MATCH_DST_MAC;
	prio->masks[0].mac = lsdn_single_mac_mask;
	lsdn_list_init(&br->if_list);

	return err;
	cleanup_ruleset:
	lsdn_ruleset_free(&br->bridge_ruleset_main);
	cleanup_if:
	acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, &sbridge_if));
	return err;
}

lsdn_err_t lsdn_sbridge_free(struct lsdn_sbridge *br)
{
	assert(lsdn_is_list_empty(&br->if_list));
	lsdn_err_t err = LSDNE_OK;
	if (!br->ctx->disable_decommit) {
		acc_inconsistent(&err, lsdn_link_delete(br->ctx->nlsock, &br->bridge_if));
	}
	lsdn_ruleset_free(&br->bridge_ruleset_main);
	lsdn_if_free(&br->bridge_if);
	return err;
}

static void mkaction_goto_br_chain(struct lsdn_filter *filter, uint16_t order, void *user)
{
	struct lsdn_sbridge_if *iface = user;
	lsdn_action_goto_chain(filter, order, iface->broadcast.chain);
}

static void mkaction_goto_switch(struct lsdn_filter *filter, uint16_t order, void *user)
{
	struct lsdn_sbridge_if *iface = user;
	lsdn_action_redir_ingress_add(filter, order, iface->bridge->bridge_if.ifindex);
}

lsdn_err_t lsdn_sbridge_add_if(struct lsdn_sbridge *br, struct lsdn_sbridge_if *iface)
{
	lsdn_err_t err;
	struct lsdn_rule* fallback = &iface->rule_fallback;
	lsdn_list_init(&iface->route_list);
	lsdn_clist_init(&iface->cl_owner, CL_OWNER);
	iface->bridge = br;

	/* create the broadcast chain */
	uint32_t br_handle;
	if(!lsdn_idalloc_get(&iface->phys_if->br_chain_ids, &br_handle))
		return LSDNE_NOMEM;
	lsdn_broadcast_init(&iface->broadcast, br->ctx, iface->phys_if->iface, br_handle);

	/* check that the ruleset is correctly setup by the caller, at least the targets */
	assert(iface->phys_if->rules_match_mac->targets[0] == LSDN_MATCH_DST_MAC);
	assert(iface->phys_if->rules_match_mac->targets[1] == iface->additional_match);
	assert(iface->phys_if->rules_fallback->targets[0] == iface->additional_match);
	assert(iface->phys_if->rules_fallback->targets[1] == LSDN_MATCH_NONE);

	/* setup basic broadcast/non-broadcast classification */
	struct lsdn_rule* match_mac = &iface->rule_match_br;
	match_mac->subprio = LSDN_SBRIDGE_IF_SUBPRIO;
	match_mac->matches[0].mac = lsdn_broadcast_mac;
	match_mac->matches[1] = iface->additional_matchdata;
	lsdn_action_init(&match_mac->action, 1, mkaction_goto_br_chain, iface);
	err = lsdn_ruleset_add(iface->phys_if->rules_match_mac, match_mac);
	if (err != LSDNE_OK)
		goto cleanup_idalloc;

	fallback->subprio = LSDN_SBRIDGE_IF_SUBPRIO;
	fallback->matches[0] = iface->additional_matchdata;
	lsdn_action_init(&fallback->action, 1, mkaction_goto_switch, iface);
	lsdn_ruleset_add(iface->phys_if->rules_fallback, fallback);
	if (err != LSDNE_OK)
		goto cleanup_match;

	/* pull broadcast rules */
	lsdn_foreach(br->if_list, if_entry, struct lsdn_sbridge_if, other_if) {
		lsdn_foreach(other_if->route_list, route_entry, struct lsdn_sbridge_route, route) {
			err = if_br_make(iface, route);
			if (err != LSDNE_OK)
				goto cleanup_clist;
		}
	}

	lsdn_list_init_add(&br->if_list, &iface->if_entry);

	return err;

	cleanup_idalloc:
	lsdn_idalloc_return(&iface->phys_if->br_chain_ids, br_handle);
	cleanup_match:
	acc_inconsistent(&err, lsdn_ruleset_remove(match_mac));
	cleanup_clist:
	acc_inconsistent(&err, lsdn_ruleset_remove(fallback));
	acc_inconsistent(&err, lsdn_clist_flush(&iface->cl_owner));
	return err;
}

lsdn_err_t lsdn_sbridge_remove_if(struct lsdn_sbridge_if *iface)
{
	assert(lsdn_is_list_empty(&iface->route_list));
	lsdn_err_t err = LSDNE_OK;
	acc_inconsistent(&err, lsdn_clist_flush(&iface->cl_owner));
	acc_inconsistent(&err, lsdn_ruleset_remove(&iface->rule_match_br));
	acc_inconsistent(&err, lsdn_ruleset_remove(&iface->rule_fallback));
	lsdn_idalloc_return(&iface->phys_if->br_chain_ids, iface->broadcast.chain);
	lsdn_broadcast_free(&iface->broadcast);

	lsdn_list_remove(&iface->if_entry);
	return err;
}

lsdn_err_t lsdn_sbridge_add_route(struct lsdn_sbridge_if *iface, struct lsdn_sbridge_route *route)
{
	lsdn_err_t err = LSDNE_OK;
	lsdn_list_init(&route->mac_list);
	lsdn_clist_init(&route->cl_dest, CL_DEST);
	route->iface = iface;

	/* push broadcast rules */
	lsdn_foreach(iface->bridge->if_list, if_entry, struct lsdn_sbridge_if, other_if) {
		if (other_if == iface)
			continue;
		err = if_br_make(other_if, route);
		if (err != LSDNE_OK)
			goto cleanup;
	}

	lsdn_list_init_add(&iface->route_list, &route->route_entry);
	return err;

	cleanup:
	return lsdn_clist_flush(&route->cl_dest);
}

lsdn_err_t lsdn_sbridge_add_route_default(struct lsdn_sbridge_if *iface, struct lsdn_sbridge_route *route)
{
	route->tunnel_action.fn = NULL;
	route->tunnel_action.actions_count = 0;
	route->tunnel_action.user = NULL;
	return lsdn_sbridge_add_route(iface, route);
}

lsdn_err_t lsdn_sbridge_remove_route(struct lsdn_sbridge_route *route)
{
	assert(lsdn_is_list_empty(&route->mac_list));
	lsdn_err_t err = lsdn_clist_flush(&route->cl_dest);
	lsdn_list_remove(&route->route_entry);
	return err;
}

lsdn_err_t lsdn_sbridge_add_mac(
	struct lsdn_sbridge_route* route, struct lsdn_sbridge_mac *mac_entry, lsdn_mac_t mac)
{
	lsdn_err_t err = LSDNE_OK;
	mac_entry->route = route;
	mac_entry->mac = mac;

	lsdn_clist_init(&mac_entry->cl_dest, CL_DEST);

	/* push forwarding rule */
	err = br_forward_make(mac_entry);
	lsdn_list_init_add(&route->mac_list, &mac_entry->mac_entry);
	return err;
}

lsdn_err_t lsdn_sbridge_remove_mac(struct lsdn_sbridge_mac *mac)
{
	lsdn_list_remove(&mac->mac_entry);
	return lsdn_clist_flush(&mac->cl_dest);
}

lsdn_err_t lsdn_sbridge_phys_if_init(
	struct lsdn_context *ctx, struct lsdn_sbridge_phys_if *sbridge_if,
	struct lsdn_if* iface, bool match_vni,
	struct lsdn_ruleset *rules_in)
{
	lsdn_err_t err = LSDNE_OK;
	sbridge_if->iface = iface;
	lsdn_idalloc_init(&sbridge_if->br_chain_ids, 1, 0xFFFF);

	// define the ruleset with priorities for match and fallback and subpriorities for us.
	// Someone else (the firewall) can share the priorities with us.
	struct lsdn_ruleset_prio *prio_match = sbridge_if->rules_match_mac =
		lsdn_ruleset_define_prio(rules_in, LSDN_IF_PRIO_MATCH);
	if(!prio_match)
		return LSDNE_NOMEM;

	struct lsdn_ruleset_prio *prio_fallback = sbridge_if->rules_fallback =
		lsdn_ruleset_define_prio(rules_in, LSDN_IF_PRIO_FALLBACK);
	if(!prio_fallback) {
		acc_inconsistent(&err, lsdn_ruleset_remove_prio(prio_match));
		return err;
	}

	prio_match->targets[0] = LSDN_MATCH_DST_MAC;
	prio_match->masks[0].mac = lsdn_multicast_mac_mask;
	if (match_vni)
		prio_match->targets[1]= LSDN_MATCH_ENC_KEY_ID;

	if (match_vni)
		prio_fallback->targets[0]= LSDN_MATCH_ENC_KEY_ID;

	err = lsdn_link_set(ctx->nlsock, iface->ifindex, true);
	if(err != LSDNE_OK) {
		acc_inconsistent(&err, lsdn_ruleset_remove_prio(prio_match));
		acc_inconsistent(&err, lsdn_ruleset_remove_prio(prio_fallback));
		return err;
	}

	return LSDNE_OK;
}

lsdn_err_t lsdn_sbridge_phys_if_free(struct lsdn_sbridge_phys_if *iface)
{
	lsdn_idalloc_free(&iface->br_chain_ids);
	return LSDNE_OK;
}

/* This are chain and priority numbers for rules on virts and shared tunnels. */
#define MATCH_PRIORITY LSDN_DEFAULT_PRIORITY
#define FALLBACK_PRIORITY (LSDN_DEFAULT_PRIORITY+1)

lsdn_err_t lsdn_sbridge_add_virt(struct lsdn_sbridge *br, struct lsdn_virt *virt)
{
	lsdn_err_t err;
	struct lsdn_context *ctx = virt->network->ctx;
	err = lsdn_prepare_rulesets(ctx, &virt->committed_if, &virt->rules_in, &virt->rules_out);
	if (err != LSDNE_OK)
		goto end;

	err = lsdn_sbridge_phys_if_init(ctx, &virt->sbridge_phys_if, &virt->committed_if, false, &virt->rules_in);
	if (err != LSDNE_OK)
		goto cleanup_rulesets;

	struct lsdn_sbridge_if *iface = &virt->sbridge_if;
	iface->phys_if = &virt->sbridge_phys_if;
	iface->additional_match = LSDN_MATCH_NONE;
	err = lsdn_sbridge_add_if(br, iface);
	if (err != LSDNE_OK)
		goto cleanup_phys_if;

	struct lsdn_sbridge_route *route = &virt->sbridge_route;
	err = lsdn_sbridge_add_route_default(iface, route);
	if (err != LSDNE_OK)
		goto cleanup_sbridge_if;

	err = lsdn_sbridge_add_mac(route, &virt->sbridge_mac, *virt->attr_mac);
	if (err != LSDNE_OK)
		goto cleanup_sbridge_route;

	return err;
	cleanup_sbridge_route:
	acc_inconsistent(&err, lsdn_sbridge_remove_route(route));
	cleanup_sbridge_if:
	acc_inconsistent(&err, lsdn_sbridge_remove_if(iface));
	cleanup_phys_if:
	acc_inconsistent(&err, lsdn_sbridge_phys_if_free(&virt->sbridge_phys_if));
	cleanup_rulesets:
	acc_inconsistent(&err, lsdn_cleanup_rulesets(ctx, &virt->committed_if, &virt->rules_in, &virt->rules_out));
	end:
	return err;
}
lsdn_err_t lsdn_sbridge_remove_virt(struct lsdn_virt *virt)
{
	lsdn_err_t err = LSDNE_OK;
	acc_inconsistent(&err, lsdn_sbridge_remove_mac(&virt->sbridge_mac));
	acc_inconsistent(&err, lsdn_sbridge_remove_route(&virt->sbridge_route));
	acc_inconsistent(&err, lsdn_sbridge_remove_if(&virt->sbridge_if));
	acc_inconsistent(&err, lsdn_sbridge_phys_if_free(&virt->sbridge_phys_if));
	// TODO: also remove the qdiscs
	lsdn_ruleset_free(&virt->rules_in);
	lsdn_ruleset_free(&virt->rules_out);
	return err;
}


lsdn_err_t lsdn_sbridge_add_stunnel(
	struct lsdn_sbridge *br, struct lsdn_sbridge_if* iface,
	struct lsdn_sbridge_phys_if *phys_if, struct lsdn_net *net)
{
	iface->phys_if = phys_if;
	iface->additional_match = LSDN_MATCH_ENC_KEY_ID;
	iface->additional_matchdata.enc_key_id = net->vnet_id;
	return lsdn_sbridge_add_if(br, iface);
}

lsdn_err_t lsdn_sbridge_remove_stunnel(struct lsdn_sbridge_if *iface)
{
	return lsdn_sbridge_remove_if(iface);
}
