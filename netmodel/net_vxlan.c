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

static void virt_add_rules(
	struct lsdn_phys_attachment *a,
	struct lsdn_if *sswitch, struct lsdn_virt *v)
{
	char buf[4096];
	char buf1[64]; lsdn_mac_to_string(&lsdn_broadcast_mac, buf1);
	char buf2[64]; lsdn_ip_to_string(a->phys->attr_ip, buf2);

	// * add rule to v matching on broadcast_mac
	//   that mirrors the packet to each other local virt
	/* TODO limit the number of actions to TCA_ACT_MAX_PRIO */
	sprintf(buf,
		"tc filter add dev %s parent ffff: protocol all prio 1 flower dst_mac %s ",
		v->connected_if.ifname, buf1);

	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_other){
		if (&v->virt_entry == &v_other->virt_entry)
			continue;
		sprintf(buf + strlen(buf),
			"action mirred egress mirror dev %s ",
			v_other->connected_if.ifname);
	}

	// * also encapsulate the packet with tunnel_key and send it to the tunnel_if
	lsdn_foreach(a->net->attached_list, attached_entry, struct lsdn_phys_attachment, a_other) {
		if (&a->phys->phys_entry == &a_other->phys->phys_entry)
			continue;
		char buf3[64]; lsdn_ip_to_string(a_other->phys->attr_ip, buf3);
		sprintf(buf + strlen(buf),
			"action tunnel_key set src_ip %s dst_ip %s id %d "
			"action mirred egress mirror dev %s ",
			buf2, buf3, a->net->vnet_id, a->tunnel->tunnel_if.ifname);
	}
	runcmd(buf);

	// * redir every other (unicast) packet to sswitch
	runcmd("tc filter add dev %s parent ffff: protocol all prio 2 flower action mirred ingress redirect dev %s",
		v->connected_if.ifname, sswitch->ifname);
}

static void static_switch_add_rules(
	struct lsdn_phys_attachment *a, struct lsdn_if *sswitch,
	struct lsdn_if *tunnel_if)
{
	// * add rule matching on dst_mac of v that just sends the packet to v
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		char buf1[64]; lsdn_mac_to_string(v->attr_mac, buf1);
		runcmd("tc filter add dev %s parent ffff: protocol all prio 1 flower dst_mac %s "
			"action mirred egress redirect dev %s",
			sswitch->ifname, buf1, v->connected_if.ifname);
	}

	lsdn_foreach(
		a->net->attached_list, attached_entry,
		struct lsdn_phys_attachment, a_other) {
		if (&a->phys->phys_entry == &a_other->phys->phys_entry)
			continue;
		lsdn_foreach(a_other->connected_virt_list, connected_virt_entry, struct lsdn_virt, v_other) {
			char buf1[64]; lsdn_mac_to_string(v_other->attr_mac, buf1);
			char buf2[64]; lsdn_ip_to_string(a->phys->attr_ip, buf2);
			char buf3[64]; lsdn_ip_to_string(a_other->phys->attr_ip, buf3);
			// * add rule for every `other_v` *not* residing on the same phys matching on dst_mac of other_v
			//   that encapsulates the packet with tunnel_key and sends it to the tunnel_if
			runcmd("tc filter add dev %s parent ffff: protocol all prio 1 flower dst_mac %s "
				"action tunnel_key set src_ip %s dst_ip %s id %d "
				"action mirred egress redirect dev %s",
				sswitch->ifname, buf1, buf2, buf3, a->net->vnet_id, tunnel_if->ifname);
		}
	}
}

static void vxlan_e2e_static_create_pa(struct lsdn_phys_attachment *a)
{
	char buf[4096];
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
		runcmd("tc qdisc add dev %s handle ffff: ingress", a->tunnel->tunnel_if.ifname);
	}

	runcmd("tc qdisc add dev %s handle ffff: ingress", sswitch_if.ifname);
	runcmd("tc qdisc add dev %s handle ffff: ingress", dummy_if.ifname);

	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		runcmd("tc qdisc add dev %s handle ffff: ingress", v->connected_if.ifname);
	}

	// redir packets outgoing from virts
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		virt_add_rules(a, &sswitch_if, v);
	}

	static_switch_add_rules(a, &sswitch_if, &a->tunnel->tunnel_if);

	// * redir broadcast packets to dummy_if
	lsdn_mac_to_string(&lsdn_broadcast_mac, buf);
	runcmd("tc filter add dev %s parent ffff: protocol all prio 1 flower "
		"dst_mac %s "
		"enc_key_id %d "
		"action mirred ingress redirect dev %s",
		a->tunnel->tunnel_if.ifname, buf, a->net->vnet_id, dummy_if.ifname);

	// * redir unicast packets to sswitch_if
	runcmd("tc filter add dev %s parent ffff: protocol all prio 2 flower "
		"enc_key_id %d "
		"action mirred ingress redirect dev %s",
		a->tunnel->tunnel_if.ifname, a->net->vnet_id, sswitch_if.ifname);

	sprintf(buf, "tc filter add dev %s parent ffff: protocol all flower ", dummy_if.ifname);
	lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
		sprintf(buf + strlen(buf), "action mirred egress mirror dev %s ", v->connected_if.ifname);
	}
	runcmd(buf);

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
