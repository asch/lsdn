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
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		&s->vxlan.mcast.mcast_ip,
		a->net->vnet_id,
		s->vxlan.port,
		true);
	if (err)
		abort();
	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);
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
			a->net->ctx->nlsock, a->tunnel.tunnel_if.ifindex,
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
		&a->tunnel.tunnel_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port,
		learning);

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
			a->net->ctx->nlsock, a->tunnel.tunnel_if.ifindex,
			!learning ? lsdn_broadcast_mac : lsdn_all_zeroes_mac,
			*a_other->phys->attr_ip);
		if (err)
			abort();
	}

	lsdn_list_init_add(&a->tunnel_list, &a->tunnel.tunnel_entry);
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

// TODO remove
static void runcmd(const char *format, ...)
{
	char cmdbuf[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(cmdbuf, sizeof(cmdbuf), format, args);
	va_end(args);
	printf("Running: %s\n", cmdbuf);
	system(cmdbuf);
}

static void redir_virt_to_static_switch(
	struct lsdn_phys_attachment *a,
	struct lsdn_if *sswitch, struct lsdn_virt *v)
{
	// * redir every packet to sswitch
	runcmd("tc filter add dev %s parent ffff: protocol all flower action mirred ingress redirect dev %s",
		v->connected_if.ifname, sswitch->ifname);
}

static void lsdn_static_switch_add_rule(
	struct lsdn_phys_attachment *a, struct lsdn_if *sswitch,
	struct lsdn_if *tunnel_if, struct lsdn_virt *v)
{
	// * add rule to sswitch matching on src_mac of v and on dst_mac of broadcast_mac
	//   that sends packet to each local virt
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_other){
		if (&v->virt_entry == &v_other->virt_entry)
			continue;
	char buf1[64]; lsdn_mac_to_string(v->attr_mac, buf1);
	char buf2[64]; lsdn_mac_to_string(&lsdn_broadcast_mac, buf2);
		runcmd("tc filter add dev %s protocol ip parent ffff: flower match src_mac %s dst_mac %s "
			"action mirred egress redirect dev %s",
			sswitch->ifname, buf1, buf2, v_other->connected_if.ifname);
	}

	// * add rule to sswitch matching on src_mac of v and on dst_mac of broadcast_mac
	//   that encapsulates the packet with tunnel_key and sends it to the tunnel_if
	lsdn_foreach(a->net->attached_list, attached_entry, struct lsdn_phys_attachment, a_other) {
		if (&a_other->phys->phys_entry == &a->phys->phys_entry)
			continue;
		char buf1[64]; lsdn_mac_to_string(v->attr_mac, buf1);
		char buf2[64]; lsdn_mac_to_string(&lsdn_broadcast_mac, buf2);
		char buf3[64]; lsdn_ip_to_string(a->phys->attr_ip, buf3);
		char buf4[64]; lsdn_ip_to_string(a_other->phys->attr_ip, buf4);
		runcmd("tc filter add dev %s protocol ip parent ffff: flower src_mac %s dst_mac %s "
			"action tunnel_key set src_ip %s dst_ip %s id %d "
			"action mirred egress redirect dev %s",
		sswitch->ifname, buf1, buf2, buf3, buf4, a->net->vnet_id, tunnel_if->ifname);
	}

	// * add rule for every `other_v` residing on the same phys matching on src_mac of v and dst_mac of other_v
	//   that just sends the packet to other_v
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_other){
		if (&v->virt_entry == &v_other->virt_entry)
			continue;
		char buf1[64]; lsdn_mac_to_string(v->attr_mac, buf1);
		char buf2[64]; lsdn_mac_to_string(v_other->attr_mac, buf2);
		runcmd("tc filter add dev %s protocol ip parent ffff: flower src_mac %s dst_mac %s "
			"action mirred egress redirect dev %s",
			sswitch->ifname, buf1, buf2, v_other->connected_if.ifname);
	}

	// * add rule for every `other_v` *not* residing on the same phys matching on src_mac of v and dst_mac of other_v
	//   that encapsulates the packet with tunnel_key and sends it to the tunnel_if
	lsdn_foreach(a->net->virt_list, virt_entry, struct lsdn_virt, v_other) {
		// TODO match on phys instead of on v
		lsdn_foreach(v_other->connected_through->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_dummy) {
			if (&v->virt_entry == &v_dummy->virt_entry)
				goto next;
		}
		char buf1[64]; lsdn_mac_to_string(v->attr_mac, buf1);
		char buf2[64]; lsdn_mac_to_string(v_other->attr_mac, buf2);
		char buf3[64]; lsdn_ip_to_string(a->phys->attr_ip, buf3);
		char buf4[64]; lsdn_ip_to_string(v->connected_through->phys->attr_ip, buf4);
		runcmd("tc filter add dev %s protocol ip parent ffff: flower src_mac %s dst_mac %s "
			"action tunnel_key set src_ip %s dst_ip %s id %d "
			"action mirred egress redirect dev %s",
			sswitch->ifname, buf1, buf2, buf3, buf4, a->net->vnet_id, tunnel_if->ifname);
next:
		;
	}
}


static void vxlan_e2e_static_create_pa(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;

	// create the static switch
	struct lsdn_if sswitch_if;
	lsdn_if_init_empty(&sswitch_if);

	int err = lsdn_link_dummy_create(ctx->nlsock, &sswitch_if, lsdn_mk_ifname(ctx));
	if (err)
		abort();

	runcmd("tc qdisc add dev %s handle ffff: ingress", sswitch_if.ifname);
	runcmd("tc qdisc add dev %s root handle 1: htb", sswitch_if.ifname);

	// conect the virts to the static switch
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		redir_virt_to_static_switch(a, &sswitch_if, v);
	}

	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		lsdn_static_switch_add_rule(a, &sswitch_if, &a->tunnel.tunnel_if, v);
	}

	a->bridge_if = sswitch_if;

	err = lsdn_link_vxlan_create(
		a->net->ctx->nlsock,
		&a->bridge_if,
		a->phys->attr_iface,
		lsdn_mk_ifname(a->net->ctx),
		NULL,
		a->net->vnet_id,
		a->net->settings->vxlan.port, true);
	if (err)
		abort();

	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other) {
		if (&a_other->phys->phys_entry == &a->phys->phys_entry)
			continue;
		err = lsdn_fdb_add_entry(a->net->ctx->nlsock, a->bridge_if.ifindex,
		lsdn_broadcast_mac, *a_other->phys->attr_ip);
		if (err)
			abort();
	}
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
