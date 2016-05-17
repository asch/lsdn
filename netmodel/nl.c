#include "nl.h"

lsdn_err_t lsdn_socket_init(struct mnl_socket **sock)
{
	struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);
	int ret;
	if (nl == NULL) {
		perror("mnl_socket_open");
		return LSDNE_BAD_SOCK;
	}

	ret = mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID);
	if (ret < 0) {
		perror("mnl_socket_bind");
		mnl_socket_close(nl);
		return LSDNE_BAD_SOCK;
	}

	*sock = nl;
	return LSDNE_OK;
}

// ip link add name <if_name> type dummy
lsdn_err_t lsdn_link_dummy_create(struct mnl_socket *sock, const char *if_name)
{
	const char *if_type = "dummy";
	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;

	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = 823;

	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	mnl_attr_put_str(nlh, IFLA_IFNAME, if_name);
	struct nlattr* nested_start = mnl_attr_nest_start(nlh, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, if_type);
	mnl_attr_nest_end(nlh, nested_start);

	// TODO: better error handling
	ret = mnl_socket_sendto(sock, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		return LSDNE_FAIL;
	}

	ret = mnl_socket_recvfrom(sock, buf, MNL_SOCKET_BUFFER_SIZE);
	if (ret == -1) {
		perror("mnl_socket_recvfrom");
		return LSDNE_FAIL;
	}

	// TODO: move response processing into separate function

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		uint32_t *err_code = mnl_nlmsg_get_payload(nlh);

		if (*err_code == 0){
			return LSDNE_OK;
		} else {
			return LSDNE_FAIL;
		}
	} else {
		return LSDNE_FAIL;
	}
}
