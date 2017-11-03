#pragma once

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
#define LSDN_NULL_HANDLE 0

/* Default priority for our fixed-function filters
 * (like those who catch ingress trafic and redirect to appropriate internal if)
 */
#define LSDN_DEFAULT_PRIORITY 10

#define LSDN_DEFAULT_CHAIN 0

/* Our egress handle (major 1, minor 0)*/
#define LSDN_DEFAULT_EGRESS_HANDLE 0x00010000U

/**
 * A handle used to identify a linux interface, stores both name and ifindex.
 *
 * An allocated lsdn_if can be in tree states:
 *  - *empty*, no name or ifindex is associated
 *  - *name*, reference an interface by name, but the interface migh not exist and ifindex is not resolved
 *  - *resolved*, references an existing interface, both name and ifindex are valid
 */
struct lsdn_if{
	unsigned int ifindex;
	char* ifname;
};

/**
 * Initialize lsdn_if in *empty* state.
 */
void lsdn_if_init(struct lsdn_if *lsdn_if);
lsdn_err_t lsdn_if_copy(struct lsdn_if *dst, struct lsdn_if *src);
/**
 * Free the underlaying memory for storing the name.
 */
void lsdn_if_free(struct lsdn_if *lsdn_if);
/**
 * Clear the name and ifindex, the handle will be in *empty* state.
 */
void lsdn_if_reset(struct lsdn_if *lsdn_if);
/**
 * Set the lsdn_if to reference a given ifname, the lsdn_if will be in *name* state.
 *
 * The ifindex may be resolved later using lsdn_if_resolve
 */
lsdn_err_t lsdn_if_set_name(struct lsdn_if *lsdn_if, const char* ifname);
/**
 * Check if the lsdn_if is in *name* or *resolved* state..
 */
static inline bool lsdn_if_is_set(const struct lsdn_if *lsdn_if)
{
	return lsdn_if->ifname;
}
/**
 * Make sure the ifindex is valid, if possible.
 *
 * If sucesfull, fills in the ifindex and moves the lsdn_if into *resolved* state.
 */
lsdn_err_t lsdn_if_resolve(struct lsdn_if *lsdn_if);

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
		lsdn_ip_t *mcast_group, uint32_t vxlanid, uint16_t port,
		bool learning, bool collect_metadata);

int lsdn_link_veth_create(struct mnl_socket *sock,
		struct lsdn_if *if1, const char *if_name1,
		struct lsdn_if *if2, const char *if_name2);

int lsdn_link_bridge_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_id,
		const char *if_name);

int lsdn_link_delete(struct mnl_socket *sock, struct lsdn_if *iface);

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

int lsdn_fdb_remove_entry(struct mnl_socket *sock, unsigned int ifindex,
			  lsdn_mac_t mac, lsdn_ip_t ip);

// filters -->
struct lsdn_filter{
	bool update;
	struct nlmsghdr *nlh;
	struct nlattr *nested_opts;
	struct nlattr *nested_acts;
};

struct lsdn_filter *lsdn_filter_flower_init(
		uint32_t if_index, uint32_t handle, uint32_t parent, uint32_t chain, uint16_t prio);
void lsdn_filter_set_update(struct lsdn_filter *f);

void lsdn_filter_free(struct lsdn_filter *f);

void lsdn_flower_actions_start(struct lsdn_filter *f);

void lsdn_filter_actions_end(struct lsdn_filter *f);

void lsdn_action_redir_ingress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex);

void lsdn_action_mirror_ingress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex);

void lsdn_action_redir_egress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex);

void lsdn_action_mirror_egress_add(
		struct lsdn_filter *f, uint16_t order, uint32_t ifindex);

void lsdn_action_set_tunnel_key(
		struct lsdn_filter *f, uint16_t order,
		uint32_t vni, lsdn_ip_t *src_ip, lsdn_ip_t *dst_ip);

void lsdn_action_drop(struct lsdn_filter *f, uint16_t order);
void lsdn_action_continue(struct lsdn_filter *f, uint16_t order);

void lsdn_action_goto_chain(struct lsdn_filter *f, uint16_t order, uint32_t chain);

void lsdn_flower_set_src_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_dst_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_enc_key_id(struct lsdn_filter *f, uint32_t vni);

void lsdn_flower_set_eth_type(struct lsdn_filter *f, uint16_t eth_type);

int lsdn_filter_create(struct mnl_socket *sock, struct lsdn_filter *f);

int lsdn_filter_delete(struct mnl_socket *sock, uint32_t ifindex, uint32_t handle,
	uint32_t parent, uint32_t chain, uint16_t prio);
