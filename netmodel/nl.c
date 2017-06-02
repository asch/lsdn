#include "private/nl.h"
#include "include/util.h"
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>
#include <linux/veth.h>
#include <assert.h>
#include <errno.h>

void lsdn_init_if(struct lsdn_if *lsdn_if)
{
	lsdn_if->ifindex = 0;
	lsdn_if->ifname[0] = 0;
}

lsdn_err_t lsdn_if_by_name(struct lsdn_if *lsdn_if, const char* ifname)
{
	if(strlen(ifname) > IF_NAMESIZE)
		return LSDNE_NOIF;

	int ifindex = if_nametoindex(ifname);
	if(ifindex == 0){
		assert(errno == ENXIO || errno == ENODEV);
		return LSDNE_NOIF;
	}

	lsdn_if->ifindex = ifindex;
	strcpy(lsdn_if->ifname, ifname);
	return LSDNE_OK;
}

struct mnl_socket *lsdn_socket_init()
{
	struct mnl_socket *sock = mnl_socket_open(NETLINK_ROUTE);
	int ret;
	if (sock == NULL) {
		perror("mnl_socket_open");
		return NULL;
	}

	ret = mnl_socket_bind(sock, 0, MNL_SOCKET_AUTOPID);
	if (ret < 0) {
		perror("mnl_socket_bind");
		mnl_socket_close(sock);
		return NULL;
	}
	return sock;
}

void lsdn_socket_free(struct mnl_socket *s)
{
    mnl_socket_close(s);
}

static void link_create_header(
		struct nlmsghdr* nlh, struct nlattr** linkinfo,
		const char *if_name, const char *if_type)
{
	assert(if_name != NULL);
	unsigned int seq = time(NULL);


	nlh->nlmsg_type	= RTM_NEWLINK;
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

static int link_create_send(
		struct mnl_socket *sock, char* buf, struct nlmsghdr *nlh,
		struct nlattr* linkinfo,
		const char *if_name, struct lsdn_if* dst_if)
{
	mnl_attr_nest_end(nlh, linkinfo);
	int ret = mnl_socket_sendto(sock, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	ret = mnl_socket_recvfrom(sock, buf, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;
	}

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type != NLMSG_ERROR) {
		return -1;
	}

	int *err_code = mnl_nlmsg_get_payload(nlh);
	if(*err_code != 0)
		return *err_code;

	lsdn_err_t lerr = lsdn_if_by_name(dst_if, if_name);
	assert(lerr == LSDNE_OK);
	return 0;
}

// ip link add name <if_name> type dummy
int lsdn_link_dummy_create(struct mnl_socket *sock, struct lsdn_if* dst_if, const char *if_name)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	link_create_header(nlh, &linkinfo, if_name, "dummy");
	return link_create_send(sock, buf, nlh, linkinfo, if_name, dst_if);
}

//ip link add link <if_name> name <vlan_name> type vlan id <vlanid>
int lsdn_link_vlan_create(struct mnl_socket *sock, struct lsdn_if* dst_if, const char *if_name,
		          const char *vlan_name, uint16_t vlanid)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	unsigned int seq = time(NULL);
	unsigned int ifindex = if_nametoindex(if_name);

	nlh->nlmsg_type	= RTM_NEWLINK;
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

//ip link add <vxlan_name> type vxlan id <vxlanid> group <mcast_group> dstport <port> dev <if_name>
int lsdn_link_vxlan_mcast_create(
	struct mnl_socket *sock, struct lsdn_if* dst_if,
	const char *if_name, const char *vxlan_name,
	lsdn_ip_t mcast_group, uint32_t vxlanid, uint16_t port)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	unsigned int seq = time(NULL);
	unsigned int ifindex = if_nametoindex(if_name);

	nlh->nlmsg_type = RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_u32(nlh, IFLA_LINK, ifindex);
	mnl_attr_put_str(nlh, IFLA_IFNAME, vxlan_name);

	linkinfo = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, "vxlan");

	struct nlattr *vxlanid_linkinfo = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	mnl_attr_put_u32(nlh, IFLA_VXLAN_ID, vxlanid);

	struct nlattr *port_linkinfo = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	mnl_attr_put_u16(nlh, IFLA_VXLAN_PORT, port);

	struct nlattr *mcast_linkinfo = mnl_attr_nest_start(nlh, IFLA_INFO_DATA);
	if (mcast_group.v == LSDN_IPv4) {
		mnl_attr_put_u32(nlh, IFLA_VXLAN_GROUP, htons(lsdn_ip4_u32(&mcast_group.v4)));
	} else {
		// TODO
	}

	mnl_attr_nest_end(nlh, mcast_linkinfo);
	mnl_attr_nest_end(nlh, port_linkinfo);
	mnl_attr_nest_end(nlh, vxlanid_linkinfo);
	mnl_attr_nest_end(nlh, linkinfo);

	return link_create_send(sock, buf, nlh, linkinfo, vxlan_name, dst_if);
}

int lsdn_link_bridge_create(struct mnl_socket *sock, struct lsdn_if* dst_if, const char *if_name)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	struct nlattr *linkinfo;

	link_create_header(nlh, &linkinfo, if_name, "bridge");
	return link_create_send(sock, buf, nlh, linkinfo, if_name, dst_if);
}

int lsdn_link_set_master(struct mnl_socket *sock,
		unsigned int master, unsigned int slave)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	int ret;
	unsigned int seq = time(NULL), change = 0;

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = change;
	ifm->ifi_flags = 0;
	ifm->ifi_index = slave;

	mnl_attr_put_u32(nlh, IFLA_MASTER, master);

	ret = mnl_socket_sendto(sock, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	ret = mnl_socket_recvfrom(sock, buf, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;
	}

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		int *err_code = mnl_nlmsg_get_payload(nlh);
		return *err_code;
	} else {
		return -1;
	}
}

int lsdn_link_veth_create(
		struct mnl_socket *sock,
		struct lsdn_if* if1, const char *if_name1,
		struct lsdn_if* if2, const char *if_name2)
{
	assert(if_name2 != NULL);
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
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

	int ret = link_create_send(sock, buf, nlh, linkinfo, if_name1, if1);
	if(ret == 0) {
		lsdn_err_t lerr = lsdn_if_by_name(if2, if_name2);
		assert(lerr == LSDNE_OK);
	}
	return ret;
}

int lsdn_link_set(struct mnl_socket *sock, unsigned int ifindex, bool up)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	int ret;
	unsigned int seq = time(NULL), change = 0, flags = 0;

	if (up) {
		change |= IFF_UP;
		flags |= IFF_UP;
	} else {
		change |= IFF_UP;
		flags &= ~IFF_UP;
	}
	
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = change;
	ifm->ifi_flags = flags;
	ifm->ifi_index = ifindex;

	ret = mnl_socket_sendto(sock, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	ret = mnl_socket_recvfrom(sock, buf, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;
	}

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		int *err_code = mnl_nlmsg_get_payload(nlh);
		return *err_code;
	} else {
		return -1;
	}
}


int lsdn_qdisc_htb_create(struct mnl_socket *sock, unsigned int ifindex,
		uint32_t parent, uint32_t handle, uint32_t r2q, uint32_t defcls)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	int ret;
	unsigned int seq = time(NULL);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = handle;
	tcm->tcm_parent = parent;

	mnl_attr_put_str(nlh, TCA_KIND, "htb");

	struct nlattr* nested_attr = mnl_attr_nest_start(nlh, TCA_OPTIONS);
	struct tc_htb_glob opts = {
		.version = TC_HTB_PROTOVER,
		.rate2quantum = r2q,
		.defcls = defcls,
		.debug = 0,
		.direct_pkts = 0
	};
	mnl_attr_put(nlh, TCA_HTB_INIT, sizeof(opts), &opts);
	mnl_attr_nest_end(nlh, nested_attr);

	ret = mnl_socket_sendto(sock, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	ret = mnl_socket_recvfrom(sock, buf, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;
	}

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		int *err_code = mnl_nlmsg_get_payload(nlh);
		return *err_code;
	} else {
		return -1;
	}
}

int lsdn_qdisc_ingress_create(struct mnl_socket *sock, unsigned int ifindex)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	bzero(buf, sizeof(buf));
	int ret;
	unsigned int seq = 0;

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = 0xFFFF0000U;
	tcm->tcm_parent = TC_H_INGRESS;

	mnl_attr_put_str(nlh, TCA_KIND, "ingress");

	ret = mnl_socket_sendto(sock, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	ret = mnl_socket_recvfrom(sock, buf, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;
	}


	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		int *err_code = mnl_nlmsg_get_payload(nlh);
		return *err_code;
	} else {
		return -1;
	}
}

// filters -->

/**
 * @brief lsdn_filter_init
 * @param kind Type of filter (e.g. "flower" or "u32")
 * @param if_index Interface index.
 * @param handle Identification of the filter. (?)
 * @param parent Handle of the qdisc.
 * @param priority Defines ordering of the filter chain.
 * @param protocol Ethertype of the filter.
 * @return Pre-prepared nl message headers and options that can be further modified.
 */
struct lsdn_filter *lsdn_filter_init(const char *kind, uint32_t if_index,
		uint32_t handle, uint32_t parent, uint16_t priority, uint16_t protocol)
{
	struct lsdn_filter *f = malloc(sizeof(*f));
	if (!f)
		return NULL;
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
	tcm->tcm_info = TC_H_MAKE(priority << 16, protocol);

	mnl_attr_put_str(f->nlh, TCA_KIND, kind);
	f->nested_opts = mnl_attr_nest_start(f->nlh, TCA_OPTIONS);
	f->nested_acts = NULL;

	return f;
}

void lsdn_filter_free(struct lsdn_filter *f)
{
	free(f->nlh);
	free(f);
}

void lsdn_filter_actions_start(struct lsdn_filter *f, uint16_t type)
{
	f->nested_acts = mnl_attr_nest_start(f->nlh, type);
}

void lsdn_filter_actions_end(struct lsdn_filter *f)
{
	mnl_attr_nest_end(f->nlh, f->nested_acts);
}

void lsdn_action_mirred_add(struct lsdn_filter *f, uint16_t order,
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

void lsdn_action_drop(struct lsdn_filter *f, uint16_t order, int action)
{
	struct nlattr* nested_attr = mnl_attr_nest_start(f->nlh, order);
	mnl_attr_put_str(f->nlh, TCA_ACT_KIND, "gact");

	struct nlattr *nested_attr2 = mnl_attr_nest_start(f->nlh, TCA_ACT_OPTIONS);
	struct tc_gact gact_act;
	bzero(&gact_act, sizeof(gact_act));
	gact_act.action = action;

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

void lsdn_flower_set_eth_type(struct lsdn_filter *f, uint16_t eth_type)
{
	mnl_attr_put_u16(f->nlh, TCA_FLOWER_KEY_ETH_TYPE, eth_type);
}

int lsdn_filter_create(struct mnl_socket *sock, struct lsdn_filter *f)
{
	unsigned seq = 0;
	f->nlh->nlmsg_type	= RTM_NEWTFILTER;
	f->nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_ACK;
	f->nlh->nlmsg_seq = seq;

	mnl_attr_nest_end(f->nlh, f->nested_opts);

	int ret = mnl_socket_sendto(sock, f->nlh, f->nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return -1;
	}

	ret = mnl_socket_recvfrom(sock, f->nlh, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return -1;
	}

	if (f->nlh->nlmsg_type == NLMSG_ERROR) {
		int *err_code = mnl_nlmsg_get_payload(f->nlh);
		return *err_code;
	} else {
		return -1;
	}
}

// <--
