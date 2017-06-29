#ifndef _LSDN_NL_H_PRIVATE_
#define _LSDN_NL_H_PRIVATE_

#include "../include/errors.h"
#include "../include/nettypes.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <libmnl/libmnl.h>

#include <net/if.h>
#include <linux/if_link.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>


/* Pseudo-handle for the linux ingress qdiscs */
#define LSDN_INGRESS_HANDLE 0xffff0000

/* Default priority for our fixed-function filters
 * (like those who catch ingress trafic and redirect to appropriate internal if)
 */
#define LSDN_DEFAULT_PRIORITY 10

/* Our egress handle (major 1, minor 0)*/
#define LSDN_DEFAULT_EGRESS_HANDLE 0x00010000U

/*
 * A handle used to identify a linux interface.
 */
struct lsdn_if{
	unsigned int ifindex;
	char* ifname;
};

void lsdn_if_init_empty(struct lsdn_if *lsdn_if);
void lsdn_if_init_known(struct lsdn_if *lsdn_if, const char* ifname, unsigned int ifindex);
lsdn_err_t lsdn_if_init_name(struct lsdn_if *lsdn_if, const char* ifname);
static inline bool lsdn_if_is_set(const struct lsdn_if *lsdn_if)
{
	return lsdn_if->ifname;
}
lsdn_err_t lsdn_if_prepare(struct lsdn_if *lsdn_if);

struct mnl_socket *lsdn_socket_init();

void lsdn_socket_free(struct mnl_socket *s);

int lsdn_link_dummy_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_if,
		const char *if_name);

int lsdn_link_vlan_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_if, const char *if_name,
		const char *vlan_name, uint16_t vlanid);

int lsdn_link_vxlan_create(struct mnl_socket *sock, struct lsdn_if* dst_if,
		const char *if_name, const char *vxlan_name,
		lsdn_ip_t *mcast_group, uint32_t vxlanid, uint16_t port, bool learning);

int lsdn_link_veth_create(struct mnl_socket *sock,
		struct lsdn_if *if1, const char *if_name1,
		struct lsdn_if *if2, const char *if_name2);

int lsdn_link_bridge_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_id,
		const char *if_name);

int lsdn_link_set_master(struct mnl_socket *sock,
		unsigned int master, unsigned int slave);

int lsdn_link_set_ip(struct mnl_socket *sock,
		const char *iface, lsdn_ip_t ip);

int lsdn_link_set(struct mnl_socket *sock, unsigned int ifindex, bool up);

int lsdn_qdisc_htb_create(struct mnl_socket *sock, unsigned int ifindex,
		uint32_t parent, uint32_t handle, uint32_t r2q, uint32_t defcls);

int lsdn_qdisc_ingress_create(struct mnl_socket *sock, unsigned int ifindex);

int lsdn_fdb_add_entry(struct mnl_socket *sock, unsigned int ifindex,
		lsdn_mac_t mac, lsdn_ip_t ip);

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

void lsdn_action_drop(struct lsdn_filter *f, uint16_t order, int action);

void lsdn_flower_set_src_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_dst_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_eth_type(struct lsdn_filter *f, uint16_t eth_type);

int lsdn_filter_create(struct mnl_socket *sock, struct lsdn_filter *f);

// <--

#endif
