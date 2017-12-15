#pragma once

#include "../include/errors.h"
#include "../include/nettypes.h"

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <libmnl/libmnl.h>
#include <linux/tc_act/tc_gact.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>


/* Pseudo-handle for the linux ingress qdiscs */
#define LSDN_INGRESS_HANDLE 0xffff0000
#define LSDN_ROOT_HANDLE 0x00010000

/* Default priority for our fixed-function filters
 * (like those who catch ingress trafic and redirect to appropriate internal if)
 */
#define LSDN_DEFAULT_PRIORITY 10

#define LSDN_DEFAULT_CHAIN 0

/**
 * A handle used to identify a linux interface, stores both name and ifindex.
 *
 * An allocated lsdn_if can be in three states:
 *  - *empty*, no name or ifindex is associated
 *  - *name*, reference an interface by name, but the interface might not exist and ifindex is not resolved
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
 * Free the underlying memory for storing the name.
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
 * Check if the lsdn_if is in *name* or *resolved* state.
 */
static inline bool lsdn_if_is_set(const struct lsdn_if *lsdn_if)
{
	return lsdn_if->ifname != NULL;
}
/**
 * Make sure the ifindex is valid, if possible.
 *
 * If sucesfull, fills in the ifindex and moves the lsdn_if into *resolved* state.
 */
lsdn_err_t lsdn_if_resolve(struct lsdn_if *lsdn_if);

struct mnl_socket *lsdn_socket_init();

void lsdn_socket_free(struct mnl_socket *s);

lsdn_err_t lsdn_link_dummy_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_if,
		const char *if_name);

lsdn_err_t lsdn_link_vlan_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_if, const char *if_name,
		const char *vlan_name, uint16_t vlanid);

lsdn_err_t lsdn_link_vxlan_create(struct mnl_socket *sock, struct lsdn_if* dst_if,
		const char *if_name, const char *vxlan_name,
		lsdn_ip_t *mcast_group, uint32_t vxlanid, uint16_t port,
		bool learning, bool collect_metadata, enum lsdn_ipv ipv);

lsdn_err_t lsdn_link_geneve_create(
	struct mnl_socket *sock, struct lsdn_if* dst_if,
	const char *new_if, uint16_t port, bool collect_metadata);

lsdn_err_t lsdn_link_veth_create(struct mnl_socket *sock,
		struct lsdn_if *if1, const char *if_name1,
		struct lsdn_if *if2, const char *if_name2);

lsdn_err_t lsdn_link_bridge_create(
		struct mnl_socket *sock,
		struct lsdn_if *dst_id,
		const char *if_name);

lsdn_err_t lsdn_link_delete(struct mnl_socket *sock, struct lsdn_if *iface);

lsdn_err_t lsdn_link_get_mtu(struct mnl_socket *sock,
		unsigned int ifindex, unsigned int *mtu);

lsdn_err_t lsdn_link_set_master(struct mnl_socket *sock,
		unsigned int master, unsigned int slave);

lsdn_err_t lsdn_link_set_ip(struct mnl_socket *sock,
		const char *iface, lsdn_ip_t ip);

lsdn_err_t lsdn_link_set(struct mnl_socket *sock, unsigned int ifindex, bool up);

lsdn_err_t lsdn_qdisc_ingress_create(struct mnl_socket *sock, unsigned int ifindex);
lsdn_err_t lsdn_qdisc_egress_create(struct mnl_socket *sock, unsigned int ifindex);

lsdn_err_t lsdn_fdb_add_entry(struct mnl_socket *sock, unsigned int ifindex,
		lsdn_mac_t mac, lsdn_ip_t ip);

lsdn_err_t lsdn_fdb_remove_entry(struct mnl_socket *sock, unsigned int ifindex,
		lsdn_mac_t mac, lsdn_ip_t ip);

struct lsdn_filter {
	/** setting this flag will replace the existing filter (if any) */
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

void lsdn_flower_actions_end(struct lsdn_filter *f);

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

/** If you want to know more, check tcf_act_police in kernel.
 *
 * The action keeps two token buckets: peak and burst. Each token corresponds to one ns elapsed.
 * Each bucket has a depletion rate -- how fast packets deplete the tokens, in order words,
 * what is a cost of a single packet. The buckets also have a maximum value, reffered to a bit
 * confusingly as MTU.
 *
 * All parameters are passed inside (struct tc_police)TCA_POLICE_TBF nlattrs, ignore the rate tables
 * in TCA_POLICE_RATE and TCA_POLICE_PEAKRATE, they are no longer used (but still needed in some
 * vestigial form).
 *
 * The parameter names are as follows:
 *
 * tc_police member  | iproute (tc) | meaning
 * --------------------------------------------------------------------------------
 * burst             | burst        | burst bucket maximum (in tokens)
 * mtu               | mtu          | peak bucket maximum (in transmit time ticks)
 * rate              | rate         | burst bucket depletion rate
 * peakrate          | peakrate     | peak bucker depletion rate
 *
 * The basic bucket you configure is the burst bucket -- this bucket controls the normal data rate
 * and has high number of tokens for bursting. In this cases, the burst rate will be unlimited.
 *
 * If you want to limit the burst rate, you use the peak token bucket, which sets the
 * absolute limit on the traffic, capping even the burst rate. You typically set the 'mtu' parameter
 * to the actual mtu, so that the peak bucket does not have any excessive bursting itself. So there
 * really are two levels of bursting, one intentional on the burst bucket, one caused by the mtu of
 * the peak bucket.
 *
 * Example traffic graph (mtu burst ommited):
 *
 * ```
 * kbps       burst
 *   ^        |---|
 *   |        #####                        -- peakrate
 *   |        #####
 *   |        ######
 *   |        #########################    -- rate
 *   |        #########################
 *   +--------------------------------------> time
 * ```
 *
 * Note: the action also has second mode, using estimators, which is less precices and the text
 * above ignores it.
 *
 * If you do not want to use \a peakrate, set it to 0.
 */
void lsdn_action_police(struct lsdn_filter *f,  uint16_t order,
	uint32_t avg_rate, uint32_t burst, uint32_t peakrate, uint32_t mtu,
	int gact_conforming, int gact_overlimit);

void lsdn_action_drop(struct lsdn_filter *f, uint16_t order);

void lsdn_action_continue(struct lsdn_filter *f, uint16_t order);

void lsdn_action_goto_chain(struct lsdn_filter *f, uint16_t order, uint32_t chain);

void lsdn_flower_set_src_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_dst_mac(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_src_ipv4(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_dst_ipv4(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_src_ipv6(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_dst_ipv6(struct lsdn_filter *f, const char *addr,
		const char *addr_mask);

void lsdn_flower_set_enc_key_id(struct lsdn_filter *f, uint32_t vni);

void lsdn_flower_set_eth_type(struct lsdn_filter *f, uint16_t eth_type);

lsdn_err_t lsdn_filter_create(struct mnl_socket *sock, struct lsdn_filter *f);

lsdn_err_t lsdn_filter_delete(struct mnl_socket *sock, uint32_t ifindex, uint32_t handle,
		uint32_t parent, uint32_t chain, uint16_t prio);
