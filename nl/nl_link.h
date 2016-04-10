#ifndef NL_LINK_H
#define NL_LINK_H

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/tc.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/class.h>
#include <netlink/route/classifier.h>
#include <netlink/route/cls/u32.h>

#include <linux/if_ether.h>

#include <unistd.h>
#include <stdio.h>

int nl_link_create_dummy(const char* if_name);

int nl_link_del(const char* if_name);

int nl_link_set_up(const char* if_name);

int nl_link_assign_addr(const char* if_name, const char* mac);

int nl_link_ingress_add_qdisc(const char* if_name);

// int nl_link_add_qdisc(const char* if_name, uint32_t parent, uint32_t handle);

// int nl_link_del_qdisc(const char* if_name, );

// Utils
struct rtnl_link *rtnl_link_dummy_alloc(void);

int u32_add_ht(struct nl_sock *sock, struct rtnl_link *rtnlLink,
		uint32_t prio, uint32_t htid, uint32_t divisor);

int get_u32(__u32 *val, const char *arg, int base);

int get_u32_handle(__u32 *handle, const char *str);
int u32_add_filter_with_hashmask(struct nl_sock *sock, struct rtnl_link *rtnlLink, uint32_t prio, 
	    uint32_t keyval, uint32_t keymask, int keyoff, int keyoffmask, struct rtnl_act *act);


#endif
