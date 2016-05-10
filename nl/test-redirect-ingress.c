#include "nl_link.h"

#include <stdio.h>

#include <linux/tc_act/tc_mirred.h>
#include <netlink/route/act/mirred.h>

int main(int argc, char ** argv){
    if (argc < 3){
        printf("Usage: %s <SOURCE DEVICE> <TARGET DEVICE>\n", argv[0]);
        return 1;
    }
	char *if_name = argv[1];

	char *if_redir_name = argv[2];

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

	struct rtnl_act *act2 = rtnl_act_alloc();
	if (!act2) {
		printf("rtnl_act_alloc() returns %p\n", act2);
		return -1;
	}
	rtnl_tc_set_kind(TC_CAST(act2), "mirred");
	rtnl_mirred_set_action(act2, TCA_EGRESS_REDIR);
	rtnl_mirred_set_policy(act2, TC_ACT_STOLEN);
	rtnl_mirred_set_ifindex(act2, rtnl_link_name2i(cache, if_redir_name));

	int ret = u32_add_filter_with_action(sk, link, 11,
			0, 0, 16, 0, act2, 0xffff, 0);

	// Create hash table with priority 10, htid 1 and divisor 50
	/*
	int htid = 1;
	int ret = u32_add_ht(sk, link, 10, htid, 50);

	char chashlink[16]="";
	strcpy(chashlink, "1:");
	uint32_t htlink = 0x0;

	if(get_u32_handle(&htlink, chashlink)) {
		printf ("Illegal \"link\"");
		nl_socket_free(sk);
		exit(1);
	}

	printf ("hash link : 0x%X\n", htlink);
	printf ("hash link test : %u\n", (htlink && TC_U32_NODE(htlink)));
	*/
    return 0;
}
