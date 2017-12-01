#include "private/nl.h"
#include "include/util.h"
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>
#include <linux/tc_act/tc_tunnel_key.h>
#include <linux/veth.h>
#include <assert.h>
#include <errno.h>

#ifdef NDEBUG
#define nl_buf(buf) \
	char buf[MNL_SOCKET_BUFFER_SIZE];
#else
#define nl_buf(buf) \
	char buf[MNL_SOCKET_BUFFER_SIZE]; \
	bzero(buf, sizeof(buf))
#endif

void lsdn_if_init(struct lsdn_if *lsdn_if)
{
	lsdn_if->ifindex = 0;
	lsdn_if->ifname = NULL;
}

lsdn_err_t lsdn_if_copy(struct lsdn_if *dst, struct lsdn_if *src)
{
	char* dup = NULL;
	if (src->ifname) {
		dup = strdup(src->ifname);
		if (!dup)
			return LSDNE_NOMEM;
	}
	lsdn_if_free(dst);
	dst->ifindex = src->ifindex;
	dst->ifname = dup;
	return LSDNE_OK;
}

void lsdn_if_free(struct lsdn_if *lsdn_if)
{
	free(lsdn_if->ifname);
}

void lsdn_if_reset(struct lsdn_if *lsdn_if)
{
	lsdn_if_free(lsdn_if);
	lsdn_if_init(lsdn_if);
}

lsdn_err_t lsdn_if_set_name(struct lsdn_if *lsdn_if, const char* ifname)
{
	char *dup = strdup(ifname);
	if (!dup)
		return LSDNE_NOMEM;

	lsdn_if_free(lsdn_if);

	lsdn_if->ifindex = 0;
	lsdn_if->ifname = dup;
	return LSDNE_OK;
}

lsdn_err_t lsdn_if_resolve(struct lsdn_if *lsdn_if)
{
	if (lsdn_if->ifindex != 0)
		return LSDNE_OK;

	int ifindex = if_nametoindex(lsdn_if->ifname);
	if(ifindex == 0){
		assert(errno == ENXIO || errno == ENODEV);
		return LSDNE_NOIF;
	}

	lsdn_if->ifindex = ifindex;

	return LSDNE_OK;
}

struct mnl_socket *lsdn_socket_init()
{
	int err;
	struct mnl_socket *sock = mnl_socket_open(NETLINK_ROUTE);
	if (!sock)
		return NULL;

	err = mnl_socket_bind(sock, 0, MNL_SOCKET_AUTOPID);
	if (err) {
		mnl_socket_close(sock);
		return NULL;
	}
	return sock;
}

void lsdn_socket_free(struct mnl_socket *s)
{
	mnl_socket_close(s);
}

static lsdn_err_t process_response(struct nlmsghdr *nlh)
{
	int *resp;
	lsdn_err_t ret;

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		resp = mnl_nlmsg_get_payload(nlh);
		if (*resp)
			ret = LSDNE_NETLINK;
		else
			ret = LSDNE_OK;
	} else {
		ret = LSDNE_NETLINK;
	}

	return ret;
}

static lsdn_err_t send_await_response(struct mnl_socket *sock, struct nlmsghdr *nlh)
{
	int ret;

	ret = mnl_socket_sendto(sock, (void *) nlh, nlh->nlmsg_len);
	if (ret == -1)
		return LSDNE_NETLINK;

	ret = mnl_socket_recvfrom(sock, (void *) nlh, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1)
		return LSDNE_NETLINK;

	return process_response(nlh);
}

static void link_create_header(
		struct nlmsghdr* nlh, struct nlattr** linkinfo,
		const char *if_name, const char *if_type)
{
	assert(if_name != NULL);
	unsigned int seq = 0;

	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_str(nlh, IFLA_IFNAME, if_name);

	*linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, if_type);
}

static lsdn_err_t link_create_send(
		struct mnl_socket *sock, char* buf, struct nlmsghdr *nlh,
		struct nlattr* linkinfo,
		const char *if_name, struct lsdn_if* dst_if)
{
	lsdn_err_t err;

	mnl_attr_nest_end(nlh, linkinfo);

	err = send_await_response(sock, nlh);
	if (err != LSDNE_OK)
		return err;

	lsdn_if_init(dst_if);

	err = lsdn_if_set_name(dst_if, if_name);
	if (err != LSDNE_OK)
		return err;

	err = lsdn_if_resolve(dst_if);

	return err;
}

// ip link add name <if_name> type dummy
lsdn_err_t lsdn_link_dummy_create(struct mnl_socket *sock, struct lsdn_if* dst_if, const char *if_name)
{
	nl_buf(buf);
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	link_create_header(nlh, &linkinfo, if_name, "dummy");
	return link_create_send(sock, buf, nlh, linkinfo, if_name, dst_if);
}

//ip link add link <if_name> name <vlan_name> type vlan id <vlanid>
lsdn_err_t lsdn_link_vlan_create(struct mnl_socket *sock, struct lsdn_if* dst_if, const char *if_name,
		const char *vlan_name, uint16_t vlanid)
{
	unsigned int seq = 0;
	nl_buf(buf);
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	unsigned int ifindex = if_nametoindex(if_name);

	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_u32(nlh, IFLA_LINK, ifindex);
	mnl_attr_put_str(nlh, IFLA_IFNAME, vlan_name);

	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "vlan");

	struct nlattr *vlanid_linkinfo = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	mnl_attr_put_u16(nlh, IFLA_VLAN_ID, vlanid);
	mnl_attr_nest_end(nlh, vlanid_linkinfo);

	mnl_attr_nest_end(nlh, linkinfo);

	return link_create_send(sock, buf, nlh, linkinfo, vlan_name, dst_if);
}

//ip link add <vxlan_name> type vxlan id <vxlanid> [group <mcast_group>] dstport <port> dev <if_name>
lsdn_err_t lsdn_link_vxlan_create(
	struct mnl_socket *sock, struct lsdn_if* dst_if,
	const char *if_name, const char *vxlan_name,
	lsdn_ip_t *mcast_group, uint32_t vxlanid, uint16_t port,
	bool learning, bool collect_metadata, enum lsdn_ipv ipv)
{
	unsigned int seq = 0;
	nl_buf(buf);
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;
	lsdn_ip_t dummy_ip;

	unsigned int ifindex = 0;
	if (if_name)
		ifindex = if_nametoindex(if_name);

	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;
	if (if_name)
		mnl_attr_put_u32(nlh, IFLA_LINK, ifindex);
	mnl_attr_put_str(nlh, IFLA_IFNAME, vxlan_name);

	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "vxlan");

	struct nlattr *vxlanid_linkinfo = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	mnl_attr_put_u32(nlh, IFLA_VXLAN_LINK, ifindex);
	mnl_attr_put_u32(nlh, IFLA_VXLAN_ID, vxlanid);
	mnl_attr_put_u16(nlh, IFLA_VXLAN_PORT, htons(port));
	mnl_attr_put_u8(nlh, IFLA_VXLAN_LEARNING, learning);
	mnl_attr_put_u8(nlh, IFLA_VXLAN_COLLECT_METADATA, collect_metadata);

	if (mcast_group) {
		if (mcast_group->v == LSDN_IPv4)
			mnl_attr_put_u32(nlh, IFLA_VXLAN_GROUP, htonl(lsdn_ip4_u32(&mcast_group->v4)));
		else
			mnl_attr_put(nlh, IFLA_VXLAN_GROUP6, sizeof(mcast_group->v6.bytes), mcast_group->v6.bytes);
	} else if (ipv == LSDN_IPv6) {
		/* We need to explicitly notify the kernel we are using IPv6 tunneling
		 * otherwise it defaults to IPv4, if neither group nor remote is
		 * specified.
		 */
		mnl_attr_put(nlh, IFLA_VXLAN_GROUP6, sizeof(dummy_ip.v6.bytes), dummy_ip.v6.bytes);
	}

	mnl_attr_nest_end(nlh, vxlanid_linkinfo);
	mnl_attr_nest_end(nlh, linkinfo);

	return link_create_send(sock, buf, nlh, linkinfo, vxlan_name, dst_if);
}

lsdn_err_t lsdn_link_bridge_create(struct mnl_socket *sock, struct lsdn_if* dst_if, const char *if_name)
{
	nl_buf(buf);
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	link_create_header(nlh, &linkinfo, if_name, "bridge");
	return link_create_send(sock, buf, nlh, linkinfo, if_name, dst_if);
}

lsdn_err_t lsdn_link_set_master(struct mnl_socket *sock,
		unsigned int master, unsigned int slave)
{
	unsigned int seq = 0, change = 0;
	nl_buf(buf);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = change;
	ifm->ifi_flags = 0;
	ifm->ifi_index = slave;

	mnl_attr_put_u32(nlh, IFLA_MASTER, master);

	return send_await_response(sock, nlh);
}

static void fdb_set_keys(struct nlmsghdr *nlh, lsdn_mac_t mac, lsdn_ip_t ip)
{
	mnl_attr_put(nlh, NDA_LLADDR, sizeof(mac.bytes), mac.bytes);

	if (ip.v == LSDN_IPv4)
		mnl_attr_put(nlh, NDA_DST, sizeof(ip.v4.bytes), ip.v4.bytes);
	else
		mnl_attr_put(nlh, NDA_DST, sizeof(ip.v6.bytes), ip.v6.bytes);
}

lsdn_err_t lsdn_fdb_add_entry(struct mnl_socket *sock, unsigned int ifindex,
		lsdn_mac_t mac, lsdn_ip_t ip)
{
	unsigned int seq = 0;
	nl_buf(buf);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWNEIGH;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_APPEND | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ndmsg *nd = mnl_nlmsg_put_extra_header(nlh, sizeof(*nd));
	nd->ndm_family = PF_BRIDGE;
	nd->ndm_state = NUD_NOARP | NUD_PERMANENT;
	nd->ndm_ifindex = ifindex;
	nd->ndm_flags = NTF_SELF;

	fdb_set_keys(nlh, mac, ip);

	return send_await_response(sock, nlh);
}

lsdn_err_t lsdn_fdb_remove_entry(struct mnl_socket *sock, unsigned int ifindex,
			  lsdn_mac_t mac, lsdn_ip_t ip)
{
	unsigned int seq = 0;
	nl_buf(buf);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_DELNEIGH;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ndmsg *nd = mnl_nlmsg_put_extra_header(nlh, sizeof(*nd));
	nd->ndm_family = PF_BRIDGE;
	nd->ndm_state = NUD_NOARP | NUD_PERMANENT;
	nd->ndm_ifindex = ifindex;
	nd->ndm_flags = NTF_SELF;

	fdb_set_keys(nlh, mac, ip);

	return send_await_response(sock, nlh);
}

lsdn_err_t lsdn_link_set_ip(struct mnl_socket *sock,
		const char *iface, lsdn_ip_t ip)
{
	unsigned int seq = 0;
	nl_buf(buf);

	int ifindex = if_nametoindex(iface);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWADDR;
	nlh->nlmsg_flags = NLM_F_REPLACE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifaddrmsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	if (ip.v == LSDN_IPv4)
		ifm->ifa_family = AF_INET;
	else
		ifm->ifa_family = AF_INET6;
	ifm->ifa_prefixlen = 24;
	ifm->ifa_index = ifindex;
	ifm->ifa_scope = 0;

	if (ip.v == LSDN_IPv4) {
		mnl_attr_put(nlh, IFA_LOCAL, sizeof(ip.v4.bytes), ip.v4.bytes);
	} else {
		mnl_attr_put(nlh, IFA_LOCAL, sizeof(ip.v6.bytes), ip.v6.bytes);
	}

	return send_await_response(sock, nlh);
}

lsdn_err_t lsdn_link_veth_create(
		struct mnl_socket *sock,
		struct lsdn_if* if1, const char *if_name1,
		struct lsdn_if* if2, const char *if_name2)
{
	lsdn_err_t err;
	nl_buf(buf);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	link_create_header(nlh, &linkinfo, if_name1, "veth");

	/* peer data */
	struct nlattr* info_data = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	struct nlattr* peer = mnl_attr_nest_start(nlh, VETH_INFO_PEER);

	struct ifinfomsg *ifm = (struct ifinfomsg*) ((char*)nlh + nlh->nlmsg_len);
	nlh->nlmsg_len += sizeof(*ifm);
	bzero(ifm, sizeof(*ifm));

	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_str(nlh, IFLA_IFNAME, if_name2);

	struct nlattr* peer_linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "veth");
	mnl_attr_nest_end(nlh, peer_linkinfo);

	mnl_attr_nest_end(nlh, peer);
	mnl_attr_nest_end(nlh, info_data);

	err = link_create_send(sock, buf, nlh, linkinfo, if_name1, if1);
	if (err == LSDNE_OK) {
		err = lsdn_if_set_name(if2, if_name2);
		if (err == LSDNE_OK)
			err = lsdn_if_resolve(if2);
	}

	return err;
}

lsdn_err_t lsdn_link_delete(struct mnl_socket *sock, struct lsdn_if *iface)
{
	unsigned int seq = 0;
	nl_buf(buf);
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);

	nlh->nlmsg_type = RTM_DELLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;
	ifm->ifi_index = iface->ifindex;

	return send_await_response(sock, nlh);
}

static int ifindex_mtu_cb(const struct nlmsghdr *nlh, void *data)
{
	int type;
	unsigned int *ifindex_mtu = (unsigned int *) data;

	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
	struct nlattr *attr;

	ifindex_mtu[0] = ifm->ifi_index;

	mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
		type = mnl_attr_get_type(attr);

		/* skip unsupported attribute in user-space */
		if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
			continue;

		if (type == IFLA_MTU)
			if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0)
				return MNL_CB_ERROR;

		ifindex_mtu[1] = mnl_attr_get_u32(attr);
	}

	return MNL_CB_OK;
}


lsdn_err_t lsdn_link_get_mtu(struct mnl_socket *sock, unsigned int ifindex,
	unsigned int *mtu)
{
	unsigned int seq = 0, portid;
	nl_buf(buf);
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	unsigned int ifindex_mtu[2];

	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq;

	struct rtgenmsg *rt = mnl_nlmsg_put_extra_header(nlh, sizeof(*rt));
	rt->rtgen_family = AF_PACKET;

	portid = mnl_socket_get_portid(sock);

	int ret = mnl_socket_sendto(sock, (void *) nlh, nlh->nlmsg_len);
	if (ret == -1)
		return LSDNE_NETLINK;

	ret = mnl_socket_recvfrom(sock, (void *) nlh, MNL_SOCKET_BUFFER_SIZE);
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, ifindex_mtu_cb, ifindex_mtu);
		if (ret <= MNL_CB_STOP)
			break;
		if (ifindex_mtu[0] == ifindex) {
			*mtu = ifindex_mtu[1];
			return LSDNE_OK;
		}
		ret = mnl_socket_recvfrom(sock, (void *) nlh, MNL_SOCKET_BUFFER_SIZE);
	}
	if (ret == -1)
		return LSDNE_NETLINK;

	return LSDNE_NOIF;
}

lsdn_err_t lsdn_link_set(struct mnl_socket *sock, unsigned int ifindex, bool up)
{
	unsigned int seq = 0, change = IFF_UP, flags = 0;
	nl_buf(buf);

	if (up)
		flags = IFF_UP;
	
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = change;
	ifm->ifi_flags = flags;
	ifm->ifi_index = ifindex;

	return send_await_response(sock, nlh);
}

lsdn_err_t lsdn_qdisc_ingress_create(struct mnl_socket *sock, unsigned int ifindex)
{
	unsigned int seq = 0;
	nl_buf(buf);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = LSDN_INGRESS_HANDLE;
	tcm->tcm_parent = TC_H_INGRESS;

	mnl_attr_put_str(nlh, TCA_KIND, "ingress");

	return send_await_response(sock, nlh);
}

lsdn_err_t lsdn_qdisc_egress_create(struct mnl_socket *sock, unsigned int ifindex)
{
	unsigned int seq = 0;
	nl_buf(buf);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = LSDN_ROOT_HANDLE;
	tcm->tcm_parent = TC_H_ROOT;

	mnl_attr_put_str(nlh, TCA_KIND, "prio");

	struct tc_prio_qopt qopt;
	qopt.bands = 2;
	bzero(qopt.priomap, sizeof(qopt.priomap));
	mnl_attr_put(nlh, TCA_OPTIONS, sizeof(qopt), &qopt);

	return send_await_response(sock, nlh);
}

/**
 * @brief lsdn_filter_init
 * @param kind Type of filter (e.g. "flower" or "u32")
 * @param if_index Interface index.
 * @param handle Identification of the filter. Assigned by kernel when handle = 0
 * @param parent Parent qdisc.
 * @param priority Defines ordering of the filter chain.
 * @param protocol Ethertype of the filter.
 * @return Pre-prepared nl message headers and options that can be further modified.
 */
static struct lsdn_filter *filter_init(const char *kind, uint32_t if_index, uint32_t handle,
		uint32_t parent, uint32_t chain, uint16_t priority)
{
	struct lsdn_filter *f = malloc(sizeof(*f));
	if (!f)
		return NULL;
	f->update = false;
	char *buf = calloc(MNL_SOCKET_BUFFER_SIZE, sizeof(char));
	if (!buf) {
		free(f);
		return NULL;
	}

	f->nlh = mnl_nlmsg_put_header(buf);

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(f->nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = if_index;
	tcm->tcm_handle = handle;
	tcm->tcm_parent = parent;
	tcm->tcm_info = TC_H_MAKE(priority << 16, ETH_P_ALL << 8);

	mnl_attr_put_str(f->nlh, TCA_KIND, kind);
	mnl_attr_put_u32(f->nlh, TCA_CHAIN, chain);
	f->nested_opts = mnl_attr_nest_start(f->nlh, TCA_OPTIONS);
	f->nested_acts = NULL;

	return f;
}

struct lsdn_filter *lsdn_filter_flower_init(uint32_t if_index, uint32_t handle, uint32_t parent, uint32_t chain, uint16_t prio)
{
	return filter_init("flower", if_index, handle, parent, chain, prio);
}


void lsdn_filter_free(struct lsdn_filter *f)
{
	free(f->nlh);
	free(f);
}

static void filter_actions_start(struct lsdn_filter *f, uint16_t type)
{
	f->nested_acts = mnl_attr_nest_start(f->nlh, type);
}

void lsdn_flower_actions_start(struct lsdn_filter *f)
{
	filter_actions_start (f, TCA_FLOWER_ACT);
}

static void filter_actions_end(struct lsdn_filter *f)
{
	mnl_attr_nest_end(f->nlh, f->nested_acts);
}

void lsdn_flower_actions_end(struct lsdn_filter *f)
{
	filter_actions_end(f);
}

static void action_mirred_add(struct lsdn_filter *f, uint16_t order,
		int action, int eaction, uint32_t ifindex)
{
	struct nlattr* nested_attr = mnl_attr_nest_start(f->nlh, order);
	mnl_attr_put_str(f->nlh, TCA_ACT_KIND, "mirred");

	struct nlattr* nested_attr2 = mnl_attr_nest_start(f->nlh, TCA_ACT_OPTIONS);

	struct tc_mirred mirred_act;
	bzero(&mirred_act, sizeof(mirred_act));
	mirred_act.action = action;
	mirred_act.eaction = eaction;
	mirred_act.ifindex = ifindex;

	mnl_attr_put(f->nlh, TCA_MIRRED_PARMS, sizeof(mirred_act), &mirred_act);

	mnl_attr_nest_end(f->nlh, nested_attr2);
	mnl_attr_nest_end(f->nlh, nested_attr);
}

void lsdn_action_redir_ingress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex)
{
	action_mirred_add(f, order, TC_ACT_STOLEN, TCA_INGRESS_REDIR, ifindex);
}

void lsdn_action_mirror_ingress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex)
{
	action_mirred_add(f, order, TC_ACT_PIPE, TCA_INGRESS_REDIR, ifindex);
}

void lsdn_action_redir_egress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex)
{
	action_mirred_add(f, order, TC_ACT_STOLEN, TCA_EGRESS_REDIR, ifindex);
}

void lsdn_action_mirror_egress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex)
{
	action_mirred_add(f, order, TC_ACT_PIPE, TCA_EGRESS_REDIR, ifindex);
}

void lsdn_action_set_tunnel_key(
		struct lsdn_filter *f, uint16_t order,
		uint32_t vni, lsdn_ip_t *src_ip, lsdn_ip_t *dst_ip)
{
	struct nlattr* nested_attr = mnl_attr_nest_start(f->nlh, order);
	mnl_attr_put_str(f->nlh, TCA_ACT_KIND, "tunnel_key");

	struct nlattr* nested_attr2 = mnl_attr_nest_start(f->nlh, TCA_ACT_OPTIONS);

	struct tc_tunnel_key tunnel_key;
	bzero(&tunnel_key, sizeof(tunnel_key));
	tunnel_key.action = TC_ACT_PIPE;
	tunnel_key.t_action = TCA_TUNNEL_KEY_ACT_SET;

	mnl_attr_put_u32(f->nlh, TCA_TUNNEL_KEY_ENC_KEY_ID, htonl(vni));
	if (src_ip->v == LSDN_IPv4 && dst_ip->v == LSDN_IPv4) {
		mnl_attr_put_u32(f->nlh, TCA_TUNNEL_KEY_ENC_IPV4_SRC, htonl(lsdn_ip4_u32(&src_ip->v4)));
		mnl_attr_put_u32(f->nlh, TCA_TUNNEL_KEY_ENC_IPV4_DST, htonl(lsdn_ip4_u32(&dst_ip->v4)));
	} else {
		mnl_attr_put(f->nlh, TCA_TUNNEL_KEY_ENC_IPV6_SRC, sizeof(src_ip->v6.bytes), src_ip->v6.bytes);
		mnl_attr_put(f->nlh, TCA_TUNNEL_KEY_ENC_IPV6_DST, sizeof(dst_ip->v6.bytes), dst_ip->v6.bytes);
	}

	mnl_attr_put(f->nlh, TCA_TUNNEL_KEY_PARMS, sizeof(tunnel_key), &tunnel_key);

	mnl_attr_nest_end(f->nlh, nested_attr2);
	mnl_attr_nest_end(f->nlh, nested_attr);
}

void lsdn_action_drop(struct lsdn_filter *f, uint16_t order)
{
	struct nlattr* nested_attr = mnl_attr_nest_start(f->nlh, order);
	mnl_attr_put_str(f->nlh, TCA_ACT_KIND, "gact");

	struct nlattr *nested_attr2 = mnl_attr_nest_start(f->nlh, TCA_ACT_OPTIONS);
	struct tc_gact gact_act;
	bzero(&gact_act, sizeof(gact_act));
	gact_act.action = TC_ACT_SHOT;

	mnl_attr_put(f->nlh, TCA_GACT_PARMS, sizeof(gact_act), &gact_act);

	mnl_attr_nest_end(f->nlh, nested_attr2);
	mnl_attr_nest_end(f->nlh, nested_attr);
}

void lsdn_action_continue(struct lsdn_filter *f, uint16_t order)
{
	struct nlattr* nested_attr = mnl_attr_nest_start(f->nlh, order);
	mnl_attr_put_str(f->nlh, TCA_ACT_KIND, "gact");

	struct nlattr *nested_attr2 = mnl_attr_nest_start(f->nlh, TCA_ACT_OPTIONS);
	struct tc_gact gact_act;
	bzero(&gact_act, sizeof(gact_act));
	gact_act.action = TC_ACT_UNSPEC;

	mnl_attr_put(f->nlh, TCA_GACT_PARMS, sizeof(gact_act), &gact_act);

	mnl_attr_nest_end(f->nlh, nested_attr2);
	mnl_attr_nest_end(f->nlh, nested_attr);
}

void lsdn_action_goto_chain(struct lsdn_filter *f, uint16_t order, uint32_t chain)
{
	struct nlattr* nested_attr = mnl_attr_nest_start(f->nlh, order);
	mnl_attr_put_str(f->nlh, TCA_ACT_KIND, "gact");

	struct nlattr *nested_attr2 = mnl_attr_nest_start(f->nlh, TCA_ACT_OPTIONS);
	struct tc_gact gact_act;
	bzero(&gact_act, sizeof(gact_act));
	gact_act.action = TC_ACT_GOTO_CHAIN | (chain & TC_ACT_EXT_VAL_MASK);

	mnl_attr_put(f->nlh, TCA_GACT_PARMS, sizeof(gact_act), &gact_act);

	mnl_attr_nest_end(f->nlh, nested_attr2);
	mnl_attr_nest_end(f->nlh, nested_attr);
}

void lsdn_flower_set_src_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask)
{
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_ETH_SRC, 6, addr);
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_ETH_SRC_MASK, 6, addr_mask);
}

void lsdn_flower_set_dst_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask)
{
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_ETH_DST, 6, addr);
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_ETH_DST_MASK, 6, addr_mask);
}

void lsdn_flower_set_src_ipv4(struct lsdn_filter *f, const char *addr,
		const char *addr_mask)
{
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV4_SRC, 4, addr);
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV4_SRC_MASK, 4, addr_mask);
}

void lsdn_flower_set_dst_ipv4(struct lsdn_filter *f, const char *addr,
		const char *addr_mask)
{
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV4_DST, 4, addr);
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV4_DST_MASK, 4, addr_mask);
}

void lsdn_flower_set_src_ipv6(struct lsdn_filter *f, const char *addr,
		const char *addr_mask)
{
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV6_SRC, 16, addr);
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV6_SRC_MASK, 16, addr_mask);
}

void lsdn_flower_set_dst_ipv6(struct lsdn_filter *f, const char *addr,
		const char *addr_mask)
{
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV6_DST, 16, addr);
	mnl_attr_put(f->nlh, TCA_FLOWER_KEY_IPV6_DST_MASK, 16, addr_mask);
}

void lsdn_flower_set_enc_key_id(struct lsdn_filter *f, uint32_t vni)
{
	mnl_attr_put_u32(f->nlh, TCA_FLOWER_KEY_ENC_KEY_ID, htonl(vni));
}

void lsdn_flower_set_eth_type(struct lsdn_filter *f, uint16_t eth_type)
{
	mnl_attr_put_u16(f->nlh, TCA_FLOWER_KEY_ETH_TYPE, eth_type);
}

lsdn_err_t lsdn_filter_create(struct mnl_socket *sock, struct lsdn_filter *f)
{
	unsigned int seq = 0;
	f->nlh->nlmsg_type = RTM_NEWTFILTER;
	f->nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_ACK;
	if (!f->update)
		f->nlh->nlmsg_flags |= NLM_F_EXCL;
	f->nlh->nlmsg_seq = seq;

	mnl_attr_nest_end(f->nlh, f->nested_opts);

	return send_await_response(sock, f->nlh);
}

/* Allow an existing TC filter to be updated. Unless this called, the filter must not exist */
void lsdn_filter_set_update(struct lsdn_filter *f)
{
	f->update = true;
}

lsdn_err_t lsdn_filter_delete(
	struct mnl_socket *sock, uint32_t ifindex, uint32_t handle,
	uint32_t parent, uint32_t chain, uint16_t prio)
{
	nl_buf(buf);
	unsigned int seq = 0;

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_DELTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = handle;
	tcm->tcm_parent = parent;
	tcm->tcm_info = TC_H_MAKE(prio << 16, ETH_P_ALL << 8);

	mnl_attr_put_u32(nlh, TCA_CHAIN, chain);

	return send_await_response(sock, nlh);
}
