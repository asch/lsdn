#include "private/net.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "include/nettypes.h"
#include "private/errors.h"
#include <stdarg.h>

static void vxlan_mcast_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_net_make_bridge(a);
	struct lsdn_settings *s = a->net->settings;
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel->tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		&s->vxlan.mcast.mcast_ip,
		a->net->vnet_id,
		s->vxlan.port,
		true,
		false);
	if (err)
		abort();
	lsdn_net_connect_bridge(a);
}

struct lsdn_net_ops lsdn_net_vxlan_mcast_ops = {
	.create_pa = vxlan_mcast_create_pa
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
			a->net->ctx->nlsock, a->tunnel->tunnel_if.ifindex,
			*v->attr_mac, *a_other->phys->attr_ip);
		if (err)
			abort();
	}
}

static void vxlan_e2e_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_net_make_bridge(a);

	bool learning = a->net->settings->switch_type == LSDN_LEARNING_E2E;
	int err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->tunnel->tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port,
		learning,
		false);

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

		err = lsdn_fdb_add_entry(
			a->net->ctx->nlsock, a->tunnel->tunnel_if.ifindex,
			!learning ? lsdn_broadcast_mac : lsdn_all_zeroes_mac,
			*a_other->phys->attr_ip);
		if (err)
			abort();
	}

	lsdn_net_connect_bridge(a);
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
	return s;
}

static void virt_add_rules(
	struct lsdn_phys_attachment *a,
	struct lsdn_if *sswitch, struct lsdn_virt *v)
{
	// * add rule to v matching on broadcast_mac
	//   that mirrors the packet to each other local virt
	/* TODO limit the number of actions to TCA_ACT_MAX_PRIO */
	struct lsdn_filter *f = lsdn_flower_init(v->connected_if.ifindex, LSDN_INGRESS_HANDLE, 1);
	lsdn_flower_set_dst_mac(f, (char *) lsdn_broadcast_mac.bytes, (char *) lsdn_single_mac_mask.bytes);
	lsdn_flower_actions_start(f);
	uint16_t order = 1;
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_other){
		if (&v->virt_entry == &v_other->virt_entry)
			continue;
		lsdn_action_mirror_egress_add(f, order++, v_other->connected_if.ifindex);
	}
	// * also encapsulate the packet with tunnel_key and send it to the tunnel_if
	lsdn_foreach(a->net->attached_list, attached_entry, struct lsdn_phys_attachment, a_other) {
		if (&a->phys->phys_entry == &a_other->phys->phys_entry)
			continue;
		lsdn_action_set_tunnel_key(f, order++, a->net->vnet_id,
		a->phys->attr_ip, a_other->phys->attr_ip);
		lsdn_action_mirror_egress_add(f, order++, a->tunnel->tunnel_if.ifindex);
	}
	lsdn_filter_actions_end(f);
	int err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_filter_free(f);

	// * redir every other (unicast) packet to sswitch
	f = lsdn_flower_init(v->connected_if.ifindex, LSDN_INGRESS_HANDLE, 2);
	lsdn_flower_actions_start(f);
	lsdn_action_redir_ingress_add(f, 1, sswitch->ifindex);
	lsdn_filter_actions_end(f);
	err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_filter_free(f);
}

static void static_switch_add_rules(
	struct lsdn_phys_attachment *a, struct lsdn_if *sswitch,
	struct lsdn_if *tunnel_if)
{
	// * add rule matching on dst_mac of v that just sends the packet to v
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		struct lsdn_filter *f = lsdn_flower_init(sswitch->ifindex, LSDN_INGRESS_HANDLE, 1);
		lsdn_flower_set_dst_mac(f, (char *) v->attr_mac->bytes, (char *) lsdn_single_mac_mask.bytes);
		lsdn_flower_actions_start(f);
		lsdn_action_redir_egress_add(f, 1, v->connected_if.ifindex);
		lsdn_filter_actions_end(f);
		int err = lsdn_filter_create(a->net->ctx->nlsock, f);
		if (err)
			abort();
		lsdn_filter_free(f);
	}

	// * add rule for every `other_v` *not* residing on the same phys matching on dst_mac of other_v
	//   that encapsulates the packet with tunnel_key and sends it to the tunnel_if
	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other) {
		if (&a->phys->phys_entry == &a_other->phys->phys_entry)
			continue;
		lsdn_foreach(a_other->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_other) {
			struct lsdn_filter *f = lsdn_flower_init(sswitch->ifindex, LSDN_INGRESS_HANDLE, 1);
			lsdn_flower_set_dst_mac(f, (char *) v_other->attr_mac->bytes, (char *) lsdn_single_mac_mask.bytes);
			lsdn_flower_actions_start(f);
			lsdn_action_set_tunnel_key(f, 1, a->net->vnet_id, a->phys->attr_ip, a_other->phys->attr_ip);
			lsdn_action_redir_egress_add(f, 2, tunnel_if->ifindex);
			lsdn_filter_actions_end(f);
			int err = lsdn_filter_create(a->net->ctx->nlsock, f);
			if (err)
				abort();
			lsdn_filter_free(f);
		}
	}
}

static void vxlan_e2e_static_create_pa(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;

	// create the static switch and the auxiliary dummy interface
	struct lsdn_if sswitch_if, dummy_if;
	lsdn_if_init_empty(&sswitch_if);
	lsdn_if_init_empty(&dummy_if);

	int err = lsdn_link_dummy_create(ctx->nlsock, &sswitch_if, lsdn_mk_ifname(ctx));
	if (err)
		abort();
	err = lsdn_link_dummy_create(ctx->nlsock, &dummy_if, lsdn_mk_ifname(ctx));
	if (err)
		abort();

	a->bridge_if = sswitch_if;
	a->dummy_if = dummy_if;

	// create the shared tunnel interface
	if (a->tunnel->tunnel_if.ifindex == 0) {
		err = lsdn_link_vxlan_create(
			ctx->nlsock,
			&a->tunnel->tunnel_if,
			a->phys->attr_iface,
			lsdn_mk_ifname(ctx),
			NULL,
			0,
			a->net->settings->vxlan.port,
			false,
			true);
		if (err)
			abort();
		err = lsdn_qdisc_ingress_create(ctx->nlsock, a->tunnel->tunnel_if.ifindex);
		if (err)
			abort();
	}

	err = lsdn_qdisc_ingress_create(ctx->nlsock, sswitch_if.ifindex);
	if (err)
		abort();
	err = lsdn_qdisc_ingress_create(ctx->nlsock, dummy_if.ifindex);
	if (err)
		abort();

	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		err = lsdn_qdisc_ingress_create(ctx->nlsock, v->connected_if.ifindex);
		if (err)
			abort();
	}

	// redir packets outgoing from virts
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		virt_add_rules(a, &sswitch_if, v);
	}

	static_switch_add_rules(a, &sswitch_if, &a->tunnel->tunnel_if);

	// * redir broadcast packets to dummy_if
	struct lsdn_filter *f = lsdn_flower_init(
		a->tunnel->tunnel_if.ifindex, LSDN_INGRESS_HANDLE, 1);
	lsdn_flower_set_dst_mac(f, (char *) lsdn_broadcast_mac.bytes, (char *) lsdn_single_mac_mask.bytes);
	lsdn_flower_set_enc_key_id(f, a->net->vnet_id);
	lsdn_flower_actions_start(f);
	lsdn_action_redir_ingress_add(f, 1, dummy_if.ifindex);
	lsdn_filter_actions_end(f);
	err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_filter_free(f);

	// * redir unicast packets to sswitch_if
	f = lsdn_flower_init(a->tunnel->tunnel_if.ifindex, LSDN_INGRESS_HANDLE, 2);
	lsdn_flower_set_enc_key_id(f, a->net->vnet_id);
	lsdn_flower_actions_start(f);
	lsdn_action_redir_ingress_add(f, 1, sswitch_if.ifindex);
	lsdn_filter_actions_end(f);
	err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_filter_free(f);

	// * copy broadcast packets to each connected virt
	f = lsdn_flower_init(dummy_if.ifindex, LSDN_INGRESS_HANDLE, 1);
	lsdn_flower_actions_start(f);
	uint16_t order = 1;
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
		lsdn_action_mirror_egress_add(f, order++, v->connected_if.ifindex);
	}
	lsdn_filter_actions_end(f);
	err = lsdn_filter_create(a->net->ctx->nlsock, f);
	if (err)
		abort();
	lsdn_filter_free(f);

	lsdn_net_connect_bridge(a);
}

struct lsdn_net_ops lsdn_net_vxlan_e2e_static_ops = {
	.create_pa = vxlan_e2e_static_create_pa
};

struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	s->ctx = ctx;
	s->switch_type = LSDN_STATIC_E2E;
	s->nettype = LSDN_NET_VXLAN;
	s->ops = &lsdn_net_vxlan_e2e_static_ops;
	s->vxlan.port = port;
	return s;
}
