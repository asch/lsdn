#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>

/**
 * Using iproute:
 * # ip link add dev <ifname> type dummy
 */

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <ifname>\n", argv[0]);
		return -1;
	}

	const char *if_name = argv[1];
	const char *if_type = "dummy";

	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;

	// Init netlink header
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWLINK;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = time(NULL);

	// Init link extra header
	struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
	ifm->ifi_family = AF_UNSPEC;
	ifm->ifi_change = 0;
	ifm->ifi_flags = 0;

	// Add attributes
	mnl_attr_put_str(nlh, IFLA_IFNAME, if_name);
	struct nlattr* nested_start = 
		mnl_attr_nest_start_check(nlh, MNL_SOCKET_BUFFER_SIZE, IFLA_LINKINFO);
	mnl_attr_put_str(nlh, IFLA_INFO_KIND, if_type);
	mnl_attr_nest_end(nlh, nested_start);

	// Open and bind socket connection
	struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	ret = mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID);
	if (ret < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}

	// Send message
	printf("Sending following message:\n");
	mnl_nlmsg_fprintf(stdout, nlh, nlh->nlmsg_len,
			  sizeof(struct ifinfomsg));

	ret = mnl_socket_sendto(nl, nlh, nlh->nlmsg_len);
	if (ret == -1) {
		perror("mnl_socket_sendto");
		exit(EXIT_FAILURE);
	}

	// Receive response
	ret = mnl_socket_recvfrom(nl, buf, MNL_SOCKET_BUFFER_SIZE);

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) {
		uint32_t *err_code = mnl_nlmsg_get_payload(nlh);

		printf ("Kernel responded with error code: %d", *err_code);
		if (*err_code == 0){
			printf (" (OK)\n");
		} else if (*err_code == -EPERM){
			printf (" (Operation not permitted)\n");
		} else if (*err_code == -EEXIST){
			printf (" (File exists)\n");
		} else if (*err_code == -EINVAL){
			printf (" (Invalid argument)\n");
		} else{
			printf ("\n");
		}
	}

	mnl_socket_close(nl);

	return 0;
}
