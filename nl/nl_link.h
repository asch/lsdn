#ifndef NL_LINK_H
#define NL_LINK_H

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/tc.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/class.h>
#include <netlink/route/classifier.h>

#include <unistd.h>
#include <stdio.h>

int nl_link_create_dummy(const char* if_name);

int nl_link_del(const char* if_name);

int nl_link_set_up(const char* if_name);

int nl_link_assign_addr(const char* if_name, const char* mac);

int nl_link_ingress_add_qdisc(const char* if_name);

int nl_link_add_qdisc(const char* if_name, uint32_t parent, uint32_t handle);

//int nl_link_del_qdisc(const char* if_name, );

// Utils
struct rtnl_link *rtnl_link_dummy_alloc(void);

#endif
