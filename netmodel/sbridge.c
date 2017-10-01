#include "private/sbridge.h"
#include "private/net.h"

static void static_bridge_add_rules(
	struct lsdn_phys_attachment *a, struct lsdn_if *sswitch,
	struct lsdn_if *tunnel_if)
{
	lsdn_ruleset_init(&a->bridge_rules, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY);

	// TODO: remember the handles
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		/* add rule matching on dst_mac of v that just sends the packet to the virt */
		struct lsdn_filter *f = lsdn_filter_flower_init(
			sswitch->ifindex, LSDN_NULL_HANDLE, LSDN_INGRESS_HANDLE,
			LSDN_DEFAULT_CHAIN, a->bridge_rules.prio);
		lsdn_flower_set_dst_mac(f, (char *) v->attr_mac->bytes, (char *) lsdn_single_mac_mask.bytes);
		lsdn_flower_actions_start(f);
		lsdn_action_redir_egress_add(f, 1, v->connected_if.ifindex);
		lsdn_filter_actions_end(f);
		int err = lsdn_filter_create(a->net->ctx->nlsock, f);
		if (err)
			abort();
		lsdn_filter_free(f);
		lsdn_ruleset_add(&a->bridge_rules, &v->owned_rules_list, 0);
	}
}


static void cb_add_virt_broadcast(
	struct lsdn_filter *filter,
	uint16_t order, void *user1, void *user2)
{
	struct lsdn_virt *v = user1;
	lsdn_action_mirror_egress_add(filter, order, v->connected_if.ifindex);
}

static void cb_add_tunneled_virt_broadcast(
	struct lsdn_filter *filter,
	uint16_t order, void *user1, void *user2)
{
	struct lsdn_phys_attachment *from = user1;
	struct lsdn_phys_attachment *to = user2;
	struct lsdn_shared_tunnel *tunnel = &from->net->settings->vxlan.e2e_static.tunnel;
	lsdn_action_set_tunnel_key(filter, order,
		from->net->vnet_id,
		from->phys->attr_ip,
		to->phys->attr_ip);
	lsdn_action_mirror_egress_add(filter, order + 1, tunnel->tunnel_if.ifindex);
}

void lsdn_sbridge_init_shared_tunnel(struct lsdn_context *ctx, struct lsdn_shared_tunnel* tun)
{
	int err = lsdn_qdisc_ingress_create(ctx->nlsock, tun->tunnel_if.ifindex);
	if (err)
		abort();
	err = lsdn_link_set(ctx->nlsock, tun->tunnel_if.ifindex, true);
	if (err)
		abort();
	lsdn_ruleset_init(&tun->rules, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY);
	lsdn_ruleset_init(&tun->broadcast_rules, LSDN_DEFAULT_CHAIN, LSDN_DEFAULT_PRIORITY);
	lsdn_idalloc_init(&tun->chain_ids, 1, 0xFFFFFFFu);
}

#define LSDN_BROADCAST_PRIORITY LSDN_DEFAULT_PRIORITY
#define LSDN_SWITCH_PRIORITY (LSDN_DEFAULT_PRIORITY+1)
#define LSDN_BROADCAST_CHAIN 1

void lsdn_sbridge_setup(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;
	// create the static switch
	struct lsdn_if sbridge_if;
	lsdn_if_init_empty(&sbridge_if);
	int err = lsdn_link_dummy_create(ctx->nlsock, &sbridge_if, lsdn_mk_ifname(ctx));
	if (err)
		abort();

	err = lsdn_qdisc_ingress_create(ctx->nlsock, sbridge_if.ifindex);
	if (err)
		abort();

	a->bridge_if = sbridge_if;

	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		// initialize the broadcast lists
		lsdn_broadcast_init(&v->broadcast_actions, LSDN_BROADCAST_CHAIN);
		// prepare qdiscs
		err = lsdn_qdisc_ingress_create(ctx->nlsock, v->connected_if.ifindex);
		if (err)
			abort();

		// if broadcast, redir packet to the broadcast chain
		struct lsdn_filter *f = lsdn_filter_flower_init(
			v->connected_if.ifindex, LSDN_NULL_HANDLE, LSDN_INGRESS_HANDLE,
			LSDN_DEFAULT_CHAIN, LSDN_BROADCAST_PRIORITY);
		lsdn_flower_set_dst_mac(f, lsdn_broadcast_mac.chr, lsdn_single_mac_mask.chr);
		lsdn_flower_actions_start(f);
		lsdn_action_goto_chain(f, 1, LSDN_BROADCAST_CHAIN);
		lsdn_filter_actions_end(f);
		err = lsdn_filter_create(a->net->ctx->nlsock, f);
		if (err)
			abort();
		lsdn_filter_free(f);

		// send anything else to the sbridge
		f = lsdn_filter_flower_init(
			v->connected_if.ifindex, LSDN_NULL_HANDLE, LSDN_INGRESS_HANDLE,
			LSDN_DEFAULT_CHAIN, LSDN_SWITCH_PRIORITY);
		lsdn_flower_actions_start(f);
		lsdn_action_redir_ingress_add(f, 1, sbridge_if.ifindex);
		lsdn_filter_actions_end(f);
		err = lsdn_filter_create(a->net->ctx->nlsock, f);
		if (err)
			abort();
		lsdn_filter_free(f);

		// add local broadcast rules to the broadcast chain
		lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v2) {
			if (v2 == v)
				continue;
			lsdn_broadcast_add(
				ctx, &v->broadcast_actions, &v2->owned_actions_list,
				&v->connected_if, cb_add_virt_broadcast, 1, v2, NULL);
		}
	}

	// fill in the actual bridging rules for the sbridge
	static_bridge_add_rules(a, &sbridge_if, &a->tunnel.tunnel_if);
}

void lsdn_sbridge_connect_shared_tunnel(
	struct lsdn_phys_attachment *a,
	struct lsdn_shared_tunnel *tunnel)
{
	int err;
	// TODO: allocate and remember the handles so that we can de-register

	// TODO: check somewhere we are not connecting to the same network twice on the local host,
	// this would lead to rule conflicts on the shared tunnel

	// redirect all broadcast packets from tunnel to our broadcast chain
	if(!lsdn_idalloc_get(&tunnel->chain_ids, &a->shared_tunnel_br_chain))
		abort();

	struct lsdn_filter *f = lsdn_filter_flower_init(
		tunnel->tunnel_if.ifindex, LSDN_NULL_HANDLE, LSDN_INGRESS_HANDLE,
		LSDN_DEFAULT_CHAIN, LSDN_BROADCAST_PRIORITY);
	lsdn_flower_set_enc_key_id(f, a->net->vnet_id);
	lsdn_flower_set_dst_mac(f, lsdn_broadcast_mac.chr, lsdn_single_mac_mask.chr);
	lsdn_flower_actions_start(f);
	lsdn_action_goto_chain(f, 1, a->shared_tunnel_br_chain);
	lsdn_filter_actions_end(f);
	err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_ruleset_add(&tunnel->broadcast_rules, &a->shared_tunnel_rules_entry, 0);
	lsdn_filter_free(f);

	// initialize the broadcast chain
	lsdn_broadcast_init(&a->broadcast_actions, a->shared_tunnel_br_chain);
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		lsdn_broadcast_add(
			a->net->ctx, &a->broadcast_actions, &v->owned_actions_list,
			&tunnel->tunnel_if, cb_add_virt_broadcast, 1, v, NULL);
	}


	// redirect all remaining packets from tunnel to the switching interface
	f = lsdn_filter_flower_init(
		tunnel->tunnel_if.ifindex, LSDN_NULL_HANDLE, LSDN_INGRESS_HANDLE,
		LSDN_DEFAULT_CHAIN, LSDN_SWITCH_PRIORITY);
	lsdn_flower_set_enc_key_id(f, a->net->vnet_id);
	lsdn_flower_actions_start(f);
	lsdn_action_redir_ingress_add(f, 1, a->bridge_if.ifindex);
	lsdn_filter_actions_end(f);
	err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_ruleset_add(&tunnel->rules, &a->shared_tunnel_rules_entry, 1);
	lsdn_filter_free(f);

	// redirect all non-local packets from the switching interface to the tunnel
	// (and add encapsulation data)
	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other)
	{
		if (&a->phys->phys_entry == &a_other->phys->phys_entry)
			continue;
		lsdn_foreach(a_other->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
			struct lsdn_filter *f = lsdn_filter_flower_init(
				a->bridge_if.ifindex, LSDN_NULL_HANDLE, LSDN_INGRESS_HANDLE,
				LSDN_DEFAULT_CHAIN, a->bridge_rules.prio);
			lsdn_flower_set_dst_mac(f, (char *) v->attr_mac->bytes, (char *) lsdn_single_mac_mask.bytes);
			lsdn_flower_actions_start(f);
			lsdn_action_set_tunnel_key(f, 1, a->net->vnet_id, a->phys->attr_ip, a_other->phys->attr_ip);
			lsdn_action_redir_egress_add(f, 2, v->network->settings->vxlan.e2e_static.tunnel.tunnel_if.ifindex);
			lsdn_filter_actions_end(f);
			int err = lsdn_filter_create(a->net->ctx->nlsock, f);
			if (err)
				abort();
			lsdn_filter_free(f);
			lsdn_ruleset_add(&a->bridge_rules, &v->owned_rules_list, 0);
		}
	}

	lsdn_list_init(&a->owned_broadcast_list);
	// add tunnel broadcast rule to virt broadcast rules
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		lsdn_foreach(v->network->attached_list, attached_entry, struct lsdn_phys_attachment, a_other) {
			if (a == a_other)
				continue;
			lsdn_broadcast_add(
				a->net->ctx, &v->broadcast_actions, &a->owned_broadcast_list,
				&v->connected_if, cb_add_tunneled_virt_broadcast, 2,
				a, a_other);
		}
	}
}
