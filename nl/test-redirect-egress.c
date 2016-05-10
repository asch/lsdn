#include "nl_link.h"

#include <stdio.h>
#include <stdlib.h>

#include <linux/tc_act/tc_mirred.h>
#include <netlink/route/act/mirred.h>

int main(int argc, char ** argv)
{
	if (argc < 4) {
		printf("Usage: %s <SOURCE DEVICE> <TARGET DEVICE> <DESTINATION MAC ADRESS>\n", argv[0]);
		return 1;
	}
	char *if_name = argv[1];

	char *if_redir_name = argv[2];
	int dst_mac_addr = atoi(argv[3]);

	struct nl_sock *sk;
	struct rtnl_link *link;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	struct nl_cache *cache;
	if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache)) < 0){
		nl_perror(err, "Unable to allocate cache");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	link = rtnl_link_get_by_name(cache, if_name);
	if (!link){
		err = NLE_OBJ_NOTFOUND;
		nl_perror(err, "Unable to modify interface");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	struct rtnl_act *act = rtnl_act_alloc();
	if (!act) {
		printf("rtnl_act_alloc() returns %p\n", act);
		return -1;
	}
	rtnl_tc_set_kind(TC_CAST(act), "mirred");
	rtnl_mirred_set_action(act, TCA_EGRESS_REDIR);
	rtnl_mirred_set_policy(act, TC_ACT_STOLEN);
	rtnl_mirred_set_ifindex(act, rtnl_link_name2i(cache, if_redir_name));

	// Add filter to qdisc with handle 1:. The filter matches on
	// the lower 4 bytes of destination MAC address.
	int ret = u32_add_filter_with_action(sk, link, 10, dst_mac_addr, 0xFFFFFFFF, -12, 0, act, 0x1, 0x0);
	if (ret != 0) {
		printf("Adding filter failed\n");
		return -1;
	}

	// Now match the upper 2 bytes of the destination MAC address.
	//ret = u32_add_filter_with_hashmask(sk, link, 10, 0x0, 0xFFFF, -14, 0, act, 0x1, 0x0);

	return 0;
}
