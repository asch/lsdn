#include "private/sbridge.h"
#include "private/net.h"

enum {CL_OWNER, CL_DEST};

/* Broadcast rule (well, action) on the sbridge_if. */
struct if_br_action {
	struct lsdn_clist_entry clist;
	struct lsdn_sbridge_route *route;
	struct lsdn_broadcast_action action;
};

static void if_br_action_free(void *user)
{
	struct if_br_action *action = user;
	lsdn_broadcast_remove(&action->action);
	free(action);
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

static void if_br_make(struct lsdn_sbridge_if *from, struct lsdn_sbridge_route *to)
{
	struct if_br_action *bra = malloc(sizeof(*bra));
	if (!bra)
		abort();
	lsdn_clist_init_entry(&bra->clist, if_br_action_free, bra);
	bra->route = to;
	struct lsdn_action_desc desc;
	/* Set tunnel metadata + mirred */
	desc.actions_count = to->tunnel_action.actions_count + 1;
	desc.fn = if_br_mkaction;
	desc.user = bra;
	lsdn_broadcast_add(&from->broadcast, &bra->action, desc);

	lsdn_clist_add(&to->cl_dest, &bra->clist);
	lsdn_clist_add(&from->cl_owner, &bra->clist);
}

/* Routing rule on the dummy bridging interface */
struct br_forward_rule {
	struct lsdn_clist_entry clist;
	struct lsdn_sbridge_mac *mac;
	struct lsdn_rule rule;
};

static void br_forward_rule_free(void *user)
{
	struct br_forward_rule *fwdr = user;
	lsdn_ruleset_remove(&fwdr->rule);
	free(fwdr);
}

static void br_forward_make(struct lsdn_sbridge_mac *mac)
{
	struct lsdn_sbridge *br = mac->route->iface->bridge;
	struct br_forward_rule *fwdr = malloc(sizeof(*fwdr));
	if (!fwdr)
		abort();
	lsdn_clist_init_entry(&fwdr->clist, br_forward_rule_free, fwdr);
	fwdr->mac = mac;
	struct lsdn_filter *f = lsdn_ruleset_add(&br->bridge_ruleset, &fwdr->rule);
	if (!f)
		abort();
	lsdn_flower_set_dst_mac(f, mac->mac.chr, lsdn_single_mac_mask.chr);
	lsdn_flower_actions_start(f);
	uint16_t order = 1;
	if (mac->route->tunnel_action.fn) {
		mac->route->tunnel_action.fn(f, order, mac->route->tunnel_action.user);
		order += mac->route->tunnel_action.actions_count;
	}
	lsdn_action_redir_egress_add(f, order, mac->route->iface->phys_if->iface->ifindex);
	lsdn_filter_actions_end(f);
	lsdn_ruleset_add_finish(&fwdr->rule);
	lsdn_clist_add(&mac->cl_dest, &fwdr->clist);
}

void lsdn_sbridge_init(struct lsdn_context *ctx, struct lsdn_sbridge *br)
{
	struct lsdn_if sbridge_if;
	lsdn_if_init(&sbridge_if);
	int err = lsdn_link_dummy_create(ctx->nlsock, &sbridge_if, lsdn_mk_ifname(ctx));
	if (err)
		abort();

	err = lsdn_qdisc_ingress_create(ctx->nlsock, sbridge_if.ifindex);
	if (err)
		abort();

	err = lsdn_link_set(ctx->nlsock, sbridge_if.ifindex, true);
	if (err)
		abort();

	br->bridge_if = sbridge_if;
	br->ctx = ctx;
	lsdn_ruleset_init(&br->bridge_ruleset, ctx, &br->bridge_if, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY);
	lsdn_list_init(&br->if_list);
}

void lsdn_sbridge_free(struct lsdn_sbridge *br)
{
	assert(lsdn_is_list_empty(&br->if_list));
	// TODO: destroy the interface
}

void lsdn_sbridge_add_if(struct lsdn_sbridge *br, struct lsdn_sbridge_if *iface)
{
	lsdn_list_init(&iface->route_list);
	lsdn_clist_init(&iface->cl_owner, CL_OWNER);
	iface->bridge = br;

	/* create the broadcast chain */
	uint32_t br_handle;
	if(!lsdn_idalloc_get(&iface->phys_if->br_chain_ids, &br_handle))
		abort();
	lsdn_broadcast_init(&iface->broadcast, br->ctx, iface->phys_if->iface, br_handle);

	/* setup basic broadcast/non-broadcast classification */
	struct lsdn_filter *f;
	f = lsdn_ruleset_add(&iface->phys_if->rules_match_mac, &iface->rule_match_br);
	if (!f)
		abort();
	if (iface->rules_filter) {
		iface->rules_filter(f, iface->rules_filter_user);
	}
	lsdn_flower_set_dst_mac(f, lsdn_broadcast_mac.chr, lsdn_single_mac_mask.chr);
	lsdn_flower_actions_start(f);
	lsdn_action_goto_chain(f, 1, iface->broadcast.chain);
	lsdn_filter_actions_end(f);
	lsdn_ruleset_add_finish(&iface->rule_match_br);

	f = lsdn_ruleset_add(&iface->phys_if->rules_fallback, &iface->rule_fallback);
	if (!f)
		abort();
	if (iface->rules_filter) {
		iface->rules_filter(f, iface->rules_filter_user);
	}
	lsdn_flower_actions_start(f);
	lsdn_action_redir_ingress_add(f, 1, br->bridge_if.ifindex);
	lsdn_filter_actions_end(f);
	lsdn_ruleset_add_finish(&iface->rule_fallback);

	/* pull broadcast rules */
	lsdn_foreach(br->if_list, if_entry, struct lsdn_sbridge_if, other_if) {
		lsdn_foreach(other_if->route_list, route_entry, struct lsdn_sbridge_route, route) {
			if_br_make(iface, route);
		}
	}

	lsdn_list_init_add(&br->if_list, &iface->if_entry);
}

void lsdn_sbridge_remove_if(struct lsdn_sbridge_if *iface)
{
	assert(lsdn_is_list_empty(&iface->route_list));
	lsdn_clist_flush(&iface->cl_owner);
	lsdn_ruleset_remove(&iface->rule_match_br);
	lsdn_ruleset_remove(&iface->rule_fallback);
	lsdn_idalloc_return(&iface->phys_if->br_chain_ids, iface->broadcast.chain);
	lsdn_broadcast_free(&iface->broadcast);

	lsdn_list_remove(&iface->if_entry);
}

void lsdn_sbridge_add_route(struct lsdn_sbridge_if *iface, struct lsdn_sbridge_route *route)
{
	lsdn_list_init(&route->mac_list);
	lsdn_clist_init(&route->cl_dest, CL_DEST);
	route->iface = iface;

	/* push broadcast rules */
	lsdn_foreach(iface->bridge->if_list, if_entry, struct lsdn_sbridge_if, other_if) {
		if (other_if == iface)
			continue;
		if_br_make(other_if, route);
	}

	lsdn_list_init_add(&iface->route_list, &route->route_entry);
}

void lsdn_sbridge_add_route_default(struct lsdn_sbridge_if *iface, struct lsdn_sbridge_route *route)
{
	route->tunnel_action.fn = NULL;
	route->tunnel_action.actions_count = 0;
	route->tunnel_action.user = NULL;
	lsdn_sbridge_add_route(iface, route);
}

void lsdn_sbridge_remove_route(struct lsdn_sbridge_route *route)
{
	assert(lsdn_is_list_empty(&route->mac_list));
	lsdn_clist_flush(&route->cl_dest);
	lsdn_list_remove(&route->route_entry);
}

void lsdn_sbridge_add_mac(
	struct lsdn_sbridge_route* route, struct lsdn_sbridge_mac *mac_entry, lsdn_mac_t mac)
{
	mac_entry->route = route;
	mac_entry->mac = mac;
	lsdn_list_init_add(&route->mac_list, &mac_entry->mac_entry);
	lsdn_clist_init(&mac_entry->cl_dest, CL_DEST);

	/* push forwarding rule */
	br_forward_make(mac_entry);
}

void lsdn_sbridge_remove_mac(struct lsdn_sbridge_mac *mac)
{
	lsdn_list_remove(&mac->mac_entry);
	lsdn_clist_flush(&mac->cl_dest);
}

void lsdn_sbridge_phys_if_init(struct lsdn_context *ctx, struct lsdn_sbridge_phys_if *sbridge_if, struct lsdn_if* iface)
{
	sbridge_if->iface = iface;
	lsdn_idalloc_init(&sbridge_if->br_chain_ids, 1, 0xFFFF);
	lsdn_ruleset_init(&sbridge_if->rules_match_mac, ctx, iface, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY);
	lsdn_ruleset_init(&sbridge_if->rules_fallback, ctx, iface, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY + 1);
	int err = lsdn_qdisc_ingress_create(ctx->nlsock, iface->ifindex);
	if (err)
		abort();
}

void lsdn_sbridge_phys_if_free(struct lsdn_sbridge_phys_if *iface)
{
	lsdn_idalloc_free(&iface->br_chain_ids);
	lsdn_ruleset_free(&iface->rules_fallback);
	lsdn_ruleset_free(&iface->rules_match_mac);
}

/* This are chain and priority numbers for rules on virts and shared tunnels. */
#define MATCH_PRIORITY LSDN_DEFAULT_PRIORITY
#define FALLBACK_PRIORITY (LSDN_DEFAULT_PRIORITY+1)

void lsdn_sbridge_add_virt(struct lsdn_sbridge *br, struct lsdn_virt *virt)
{
	struct lsdn_context *ctx = virt->network->ctx;
	lsdn_sbridge_phys_if_init(ctx, &virt->sbridge_phys_if, &virt->committed_if);

	struct lsdn_sbridge_if *iface = &virt->sbridge_if;
	iface->phys_if = &virt->sbridge_phys_if;
	iface->rules_filter = NULL;
	lsdn_sbridge_add_if(br, iface);

	struct lsdn_sbridge_route *route = &virt->sbridge_route;
	lsdn_sbridge_add_route_default(iface, route);

	lsdn_sbridge_add_mac(route, &virt->sbridge_mac, *virt->attr_mac);
}
void lsdn_sbridge_remove_virt(struct lsdn_virt *virt)
{
	lsdn_sbridge_remove_mac(&virt->sbridge_mac);
	lsdn_sbridge_remove_route(&virt->sbridge_route);
	lsdn_sbridge_remove_if(&virt->sbridge_if);
}

static void stunnel_filter(struct lsdn_filter *f, void *user)
{
	struct lsdn_net *net = user;
	lsdn_flower_set_enc_key_id(f, net->vnet_id);
}

void lsdn_sbridge_add_stunnel(struct lsdn_sbridge *br, struct lsdn_sbridge_if* iface, struct lsdn_sbridge_phys_if *phys_if, struct lsdn_net *net)
{
	iface->phys_if = phys_if;
	iface->rules_filter = stunnel_filter;
	iface->rules_filter_user = net;
	lsdn_sbridge_add_if(br, iface);
}

void lsdn_sbridge_remove_stunnel(struct lsdn_sbridge_if *iface)
{
	lsdn_sbridge_remove_if(iface);
}
