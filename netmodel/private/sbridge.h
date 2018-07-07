#pragma once

#include "list.h"
#include "idalloc.h"
#include "nl.h"
#include "rules.h"
#include "clist.h"

/* Helpers for implementing a TC based static switching mechanism with support
 * for tunnel metadata tagging */

struct lsdn_sbridge;
struct lsdn_sbridge_if;
struct lsdn_sbridge_if_route;

/* A single static bridge */
struct lsdn_sbridge {
	struct lsdn_list_entry if_list;
	struct lsdn_context *ctx;

	struct lsdn_if bridge_if;
	struct lsdn_ruleset bridge_ruleset_main;
	struct lsdn_ruleset_prio *bridge_ruleset;
};

typedef void (*lsdn_mkmatch_cb)(struct lsdn_filter *f, void *user);

/* An interface connected to the static bridge.
 *
 * Multiple possible outgoing routes can be associated with this interface.*/
struct lsdn_sbridge_if {
	/* The user of the sbridge API must provide a sbridge_phys_if. The physical interface
	 * can be shared among multiple users, in that case use rules_filter=true when sharing the chain,
	 * or set it to false if not.
	 */
	struct lsdn_sbridge_phys_if *phys_if;

	enum lsdn_rule_target additional_match;
	union lsdn_matchdata additional_matchdata;

	/* Private part starts here */
	struct lsdn_broadcast broadcast;
	struct lsdn_sbridge *bridge;
	struct lsdn_list_entry if_entry;
	struct lsdn_list_entry route_list;
	struct lsdn_clist cl_owner;

	struct lsdn_rule rule_match_br;
	struct lsdn_rule rule_fallback;
};

struct lsdn_sbridge_phys_if {
	/* TODO: Lbridge now can use it to, rename it and move it */
	struct lsdn_if *iface;
	struct lsdn_idalloc br_chain_ids;
	struct lsdn_ruleset_prio *rules_match_mac;
	struct lsdn_ruleset_prio *rules_fallback;
	struct lsdn_ruleset_prio *rules_source;
};

/* Outgoing route from a bridge. because data outgoing from the bridge sometimes need a tunneling metadata,
 * so there might be multiple routes for single lsdn_sbridge_if.
 */
struct lsdn_sbridge_route {
	/* Callback to add an action setting the tunnel metadata.*/
	struct lsdn_action_desc tunnel_action;

	/* Private part starts here */
	struct lsdn_list_entry route_entry;
	struct lsdn_sbridge_if *iface;
	struct lsdn_list_entry mac_list;
	struct lsdn_clist cl_dest;
};

struct lsdn_sbridge_mac {
	struct lsdn_sbridge_route *route;
	struct lsdn_list_entry mac_entry;
	lsdn_mac_t mac;
	struct lsdn_clist cl_dest;
};

/* Create a bridge using tc rules to route the packets between it's interfaces. Since the bridge
 * is not learning, each interface must have its associated mac addresses. */
lsdn_err_t lsdn_sbridge_init(struct lsdn_context *ctx, struct lsdn_sbridge *br);
lsdn_err_t lsdn_sbridge_free(struct lsdn_sbridge *br);
lsdn_err_t lsdn_sbridge_add_if(struct lsdn_sbridge *br, struct lsdn_sbridge_if *iface);
lsdn_err_t lsdn_sbridge_remove_if(struct lsdn_sbridge_if *iface);
lsdn_err_t lsdn_sbridge_add_route(struct lsdn_sbridge_if *iface, struct lsdn_sbridge_route *route);
lsdn_err_t lsdn_sbridge_add_route_default(struct lsdn_sbridge_if *iface, struct lsdn_sbridge_route *route);
lsdn_err_t lsdn_sbridge_remove_route(struct lsdn_sbridge_route *route);
lsdn_err_t lsdn_sbridge_add_mac(
	struct lsdn_sbridge_route* route, struct lsdn_sbridge_mac *mac_entry, lsdn_mac_t mac);
lsdn_err_t lsdn_sbridge_remove_mac(struct lsdn_sbridge_mac *mac);
lsdn_err_t lsdn_sbridge_phys_if_init(
	struct lsdn_context *ctx, struct lsdn_sbridge_phys_if *sbridge_if,
	struct lsdn_if* iface, bool match_vni,
	struct lsdn_ruleset *rules_in);
lsdn_err_t lsdn_sbridge_phys_if_free(struct lsdn_sbridge_phys_if *iface);

struct lsdn_virt;
struct lsdn_phys_attachment;
struct lsdn_net;

lsdn_err_t lsdn_sbridge_add_virt(struct lsdn_sbridge *br, struct lsdn_virt *virt);
lsdn_err_t lsdn_sbridge_remove_virt(struct lsdn_virt *virt);

lsdn_err_t lsdn_sbridge_add_stunnel(
		struct lsdn_sbridge *br, struct lsdn_sbridge_if* iface,
		struct lsdn_sbridge_phys_if *tunnel, struct lsdn_net *net);
lsdn_err_t lsdn_sbridge_remove_stunnel(
		struct lsdn_sbridge_if *iface);
