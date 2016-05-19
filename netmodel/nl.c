#include "nl.h"

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

// ip link add name <if_name> type dummy
int lsdn_link_dummy_create(struct mnl_socket *sock, const char *if_name)
{
	const char *if_type = "dummy";
	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;
	unsigned int seq = time(NULL);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_str(nlh, IFLA_IFNAME, if_name);
	struct nlattr* nested_start = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, if_type);
	mnl_attr_nest_end(nlh, nested_start);

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

int lsdn_link_set(struct mnl_socket *sock, const char *if_name, bool up)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
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

	mnl_attr_put_str(nlh, IFLA_IFNAME, if_name);

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


int lsdn_qdisc_htb_create(struct mnl_socket *sock, unsigned int if_index,
		uint32_t parent, uint32_t handle, uint32_t r2q, uint32_t defcls)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;
	unsigned int seq = time(NULL);

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = if_index;
	tcm->tcm_handle = handle;
	tcm->tcm_parent = parent;

	mnl_attr_put_str(nlh, TCA_KIND, "htb");

	struct nlattr* nested_attr = mnl_attr_nest_start(nlh, TCA_OPTIONS);
	struct tc_htb_glob opts = {
		.version = TC_HTB_PROTOVER,
		.rate2quantum = r2q,
		.defcls = defcls,
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
