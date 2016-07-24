#ifndef _LSDN_NL_H
#define _LSDN_NL_H

#include "errors.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>

#include <libmnl/libmnl.h>

#include <net/if.h>
#include <linux/if_link.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/tc_act/tc_mirred.h>


struct mnl_socket *lsdn_socket_init();

void lsdn_socket_free(struct mnl_socket *s);

int lsdn_link_dummy_create(struct mnl_socket *sock, const char *if_name);

int lsdn_link_set(struct mnl_socket *sock, const char *if_name, bool up);

int lsdn_qdisc_htb_create(struct mnl_socket *sock, unsigned if_index,
		uint32_t parent, uint32_t handle, uint32_t r2q, uint32_t defcls);

int lsdn_qdisc_ingress_create(struct mnl_socket *sock, unsigned if_index);


// filters -->
struct lsdn_filter{
	struct nlmsghdr *nlh;
	struct nlattr *nested_opts;
	struct nlattr *nested_acts;
};

struct lsdn_filter *lsdn_filter_init(const char *kind, uint32_t if_index,
		uint32_t handle, uint32_t parent, uint16_t priority, uint16_t protocol);

void lsdn_filter_free(struct lsdn_filter *f);

void lsdn_filter_actions_start(struct lsdn_filter *f, uint16_t type);

void lsdn_filter_actions_end(struct lsdn_filter *f);

void lsdn_action_mirred_add(struct lsdn_filter *f, uint16_t order,
		int action, int eaction, uint32_t ifindex);

void lsdn_flower_set_src_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_dst_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_eth_type(struct lsdn_filter *f, uint16_t eth_type);

int lsdn_filter_create(struct mnl_socket *sock, struct lsdn_filter *f);

// <--

#endif
