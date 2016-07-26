#include "../netmodel/private/nl.h"

#include <stdio.h>

int main(){
	struct mnl_socket *sock = lsdn_socket_init();
	if (sock == NULL) {
		return -1;
	}

	int err;
	const char *ifname = "test_dummy";
	const char *ifname_dst = "test_dummy_dst";
	struct lsdn_if lsdn_if;
	struct lsdn_if lsdn_if_dst;

	// Create interface 'test_dummy'
	// (# ip link add dev test_dummy type dummy)
	printf("Creating interface '%s': ", ifname);

	err = lsdn_link_dummy_create(sock, &lsdn_if, ifname);
	printf("%d (%s)\n", err, strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	// Create interface 'test_dummy_dst'
	// (# ip link add dev test_dummy_dst type dummy)
	printf("Creating interface '%s': ", ifname_dst);
	err = lsdn_link_dummy_create(sock, &lsdn_if_dst, ifname_dst);
	printf("%d (%s)\n", err, strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	// Add HTB qdisc
	// (# tc qdisc add dev test_dummy root handle 1: htb default 0 r2q 10)
	printf("Adding HTB qdisc: ");
	err = lsdn_qdisc_htb_create(sock, lsdn_if.ifindex,
			TC_H_ROOT, 0x00010000U, 10, 0);
	printf("%d (%s)\n", err, strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	// Set interface up
	// (# ip link set test_dummy up)
	printf("Setting interface up: ");
	err = lsdn_link_set(sock, lsdn_if.ifindex, true);
	printf("%d (%s)\n", err, strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	// Add ingress qdisc
	// (# tc qdisc add dev test_dummy ingress)
	printf("Adding ingress qdisc: ");
	err = lsdn_qdisc_ingress_create(sock, lsdn_if.ifindex);
	printf("%d (%s)\n", err, strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	// Redirect traffic from 'test_dummy' ingress to 'test_dummy_dst'
	// (# tc filter add dev test_dummy handle 1 parent ffff: protocol all prio 10 flower src_mac 30:31:32:33:34:35 action mirred egress redirect dev test_dummy_dst)
	// Note: magic is needed for exact match with tc command
	printf("Adding flower filter for '%s': ", ifname);
	uint32_t filter_handle = 0x00000001U;
	uint32_t parent_handle = 0xffff0000U;

	struct lsdn_filter *filter = lsdn_filter_init("flower", lsdn_if.ifindex,
			filter_handle, parent_handle, 10, ETH_P_ALL<<8 /* magic trick 1 */);

	lsdn_filter_actions_start(filter, TCA_FLOWER_ACT);
	lsdn_action_mirred_add(filter, 1, TC_ACT_STOLEN, TCA_EGRESS_REDIR,
			lsdn_if_dst.ifindex);
	lsdn_filter_actions_end(filter);

	const char addr_mask[] = {255, 255, 255, 255, 255, 255};
	lsdn_flower_set_src_mac(filter, "012345", addr_mask);
	lsdn_flower_set_eth_type(filter, ETH_P_ALL<<8 /* magic trick 2 */);

	err = lsdn_filter_create(sock, filter);
	printf("%d (%s)\n", err, strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	lsdn_filter_free(filter);


	mnl_socket_close(sock);

	return 0;
}
