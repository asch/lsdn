#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>

/**
 * Using iproute:
 * # ip link add dev <if_name> type dummy
 *
 * note: for simplicity this program uses if_index insted of if_name
 * TODO: use if_name instead of if_index
 */

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <if_index>\n", argv[0]);
		return -1;
	}

	size_t if_index = atoi(argv[1]);
	const char *tc_kind = "htb";

	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;

	// Init netlink header
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type	= RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = time(NULL);

	// Init extra header
	struct tcmsg *tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	// tcm->tcm_family = AF_INET;
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = if_index;
	// TODO: report bug - macros for handle manipulation are useless
	tcm->tcm_handle = 0x00010000U;
	tcm->tcm_parent = TC_H_ROOT;

	// Add attributes
	mnl_attr_put_str(nlh, TCA_KIND, tc_kind);
	struct nlattr* nested_attr = mnl_attr_nest_start(nlh, TCA_OPTIONS);

	struct tc_htb_glob opts = {
		.version = TC_HTB_PROTOVER,
		.rate2quantum = 10,
	};
	mnl_attr_put(nlh, TCA_HTB_INIT, sizeof(opts), &opts);
	mnl_attr_nest_end(nlh, nested_attr);

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
	// BUG: prints only first 4 words of extra header, tcmsg has 5 words

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

