#ifndef _LSDN_H
#define _LSDN_H

#include <stdint.h>
#include <stdbool.h>
#include "../private/list.h"
#include "../private/rule.h"
#include "../private/names.h"
#include "nettypes.h"

#define LSDN_DECLARE_ATTR(obj, name, type) \
	lsdn_err_t lsdn_##obj##_set_##name(struct lsdn_##obj *obj, type value); \
	lsdn_err_t lsdn_##obj##_clear_##name(struct lsdn_##obj *obj); \
	type lsdn_##obj##_get_##name(struct lsdn_##obj *obj)

typedef void (*lsdn_nomem_cb)(void *user);

/**
 * A top-level object encompassing all network topology. This includes virtual networks
 * (lsdn_network) and physical host connections (lsdn_phys). Only one context will typically exist
 * in a given program.
 *
 * The same structures (lsdn_phys, lsdn_virt) are used to describe both remote objects
 * and objects running on other machines. This allows the orchestrator to make the same API calls
 * on all physical machines to construct the network topology. The only difference between the
 * API calls on the physical machines will be the lsdn_phys_claim_local calls.
 */
struct lsdn_context{
	/* Determines the prefix for interfaces created in the context */
	char* name;
	lsdn_nomem_cb nomem_cb;
	void *nomem_cb_user;
	struct lsdn_names phys_names;
	struct lsdn_names net_names;

	struct lsdn_list_entry networks_list;
	struct lsdn_list_entry settings_list;
	struct lsdn_list_entry phys_list;
	struct mnl_socket *nlsock;

	// error handling -- only valid during validation and commit
	struct lsdn_problem problem;
	struct lsdn_problem_ref problem_refs[LSDN_MAX_PROBLEM_REFS];
	lsdn_problem_cb problem_cb;
	void *problem_cb_user;
	size_t problem_count;

	int ifcount;
	char namebuf[IF_NAMESIZE + 1];
};

#include "../private/errors.h"

struct lsdn_context *lsdn_context_new(const char* name);
void lsdn_context_set_nomem_callback(struct lsdn_context *ctx, lsdn_nomem_cb cb, void *user);
void lsdn_context_abort_on_nomem(struct lsdn_context *ctx);
void lsdn_context_free(struct lsdn_context *ctx);

enum lsdn_nettype{
	LSDN_NET_VXLAN, LSDN_NET_VLAN, LSDN_NET_DIRECT
};

enum lsdn_switch{
	/* A learning switch with single tunnel shared from the phys.
	 *
	 * The network is essentially autoconfiguring in this mode.
	 */
	LSDN_LEARNING,
	/* A learning switch with a tunnel for each connected endpoint
	 *
	 * In this mode the connection information (IP addr) for each physical node is required.
	 */
	LSDN_LEARNING_E2E,
	/* Static switching with a tunnel for each connected endpoint
	 * Note: the endpoint is represented by a single linux interface,
	 * with the actual endpoint being selected by tc actions.
	 *
	 * In this mode we need the connection information + MAC addresses of all virts and where
	 * they reside.
	 */
	LSDN_STATIC_E2E

	/* LSDN_STATIC does not exists, because it does not make much sense ATM. It would have
	 * static rules for the switching at local level, but it would go out through a single
	 * interface to be switched by some sort of learning switch. May be added if it appears.
	 */
};

/**
 * Defines the type of a lsdn_net. Multiple networks can share the same settings
 * (e.g. vxlan with static routing on port 1234) and only differ by their identifier
 * (vlan id, vni ...).
 */
struct lsdn_settings{
	struct lsdn_list_entry settings_entry;
	struct lsdn_context *ctx;
	struct lsdn_net_ops *ops;

	enum lsdn_nettype nettype;
	enum lsdn_switch switch_type;
	union {
		struct {
			uint16_t port;
			union{
				lsdn_ip_t mcast_ip;
			} mcast;
		} vxlan;
	};
};

struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx);
struct lsdn_settings *lsdn_settings_new_vxlan_mcast(struct lsdn_context *ctx, lsdn_ip_t mcast_ip, uint16_t port);
struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port);
struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port);
void lsdn_settings_free(struct lsdn_settings *settings);

/**
 * Virtual network to which nodes (lsdn_virt) connect through physical host connections (lsdn_phys).
 * Can be implemented using common tunneling techniques, like vlan or vxlan or no tunneling.
 *
 * Networks are defined by two main characteristics:
 *  - the tunnel used to overlay the network over physical topology (transparent to end users)
 *  - the switching methods used (visible to end users)
 */
struct lsdn_net {
	struct lsdn_list_entry networks_entry;
	struct lsdn_context *ctx;
	struct lsdn_settings *settings;
	struct lsdn_name name;

	uint32_t vnet_id;
	struct lsdn_list_entry virt_list;
	/* List of lsdn_phys_attachement attached to this network */
	struct lsdn_list_entry attached_list;

};

struct lsdn_net *lsdn_net_new(struct lsdn_settings *settings, uint32_t vnet_id);
lsdn_err_t lsdn_net_set_name(struct lsdn_net *net, const char *name);
const char* lsdn_net_get_name(struct lsdn_net *net);
struct lsdn_net* lsdn_net_by_name(struct lsdn_context *ctx, const char *name);
void lsdn_net_free(struct lsdn_net *net);

/**
 * Represents a physical host connection (e.g. eth0 on lsdn1).
 * Physical interfaces are used to connect to virtual networks. This connection is called
 * lsdn_phys_attachement.
 */
struct lsdn_phys {
	struct lsdn_name name;
	struct lsdn_list_entry phys_entry;
	struct lsdn_list_entry attached_to_list;

	struct lsdn_context* ctx;
	bool is_local;
	char *attr_iface;
	lsdn_ip_t *attr_ip;
};

struct lsdn_phys *lsdn_phys_new(struct lsdn_context *ctx);
lsdn_err_t lsdn_phys_set_name(struct lsdn_phys *phys, const char *name);
const char* lsdn_phys_get_name(struct lsdn_phys *phys);
struct lsdn_phys* lsdn_phys_by_name(struct lsdn_context *ctx, const char *name);
void lsdn_phys_free(struct lsdn_phys *phys);
/* TODO: provide a way to get missing attributes */
lsdn_err_t lsdn_phys_attach(struct lsdn_phys *phys, struct lsdn_net* net);
lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys);

LSDN_DECLARE_ATTR(phys, ip, lsdn_ip_t);
LSDN_DECLARE_ATTR(phys, iface, const char*);

struct lsdn_tunnel {
	struct lsdn_if tunnel_if;
	struct lsdn_list_entry tunnel_entry;
};

/**
 * A point of connection to a virtual network through a physical interface.
 * Only single attachment may exist for a pair of a physical connection and network.
 */
struct lsdn_phys_attachment {
	/* list held by net */
	struct lsdn_list_entry attached_entry;
	/* list held by phys */
	struct lsdn_list_entry attached_to_entry;
	struct lsdn_list_entry connected_virt_list;

	struct lsdn_net *net;
	struct lsdn_phys *phys;
	/* Was this attachment created by lsdn_phys_attach at some point, or was it implicitely
	 * created by lsdn_virt_connect, just for bookkeeping?
	 */
	bool explicitely_attached;

	/* Either a dummy interface for static switch or linux bridge for learning networks */
	struct lsdn_if bridge_if;
	union {
		/* Only valid for LSDN_STATIC_E2E */
		struct lsdn_if dummy_if;
	};
	struct lsdn_list_entry tunnel_list;
	/* for free internal use by the network */
	struct lsdn_tunnel *tunnel;
};


/**
 * A virtual machine (typically -- it may be any linux interface).
 *
 * Virtual machines participate in virtual networks (through phys_attachments on the host machine
 * connection). They can be migrated between the physical machines by connecting them through
 * different lsdn_phys.
 */
struct lsdn_virt {
	struct lsdn_list_entry virt_entry;
	struct lsdn_list_entry connected_virt_entry;
	struct lsdn_net* network;

	struct lsdn_list_entry virt_rules_list;

	struct lsdn_phys_attachment* connected_through;
	struct lsdn_if connected_if;

	lsdn_mac_t *attr_mac;
	/*lsdn_ip_t *attr_ip; */
};

struct lsdn_virt *lsdn_virt_new(struct lsdn_net *net);
void lsdn_virt_free(struct lsdn_virt* vsirt);
lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt *virt, struct lsdn_phys *phys, const char *iface);
void lsdn_virt_disconnect(struct lsdn_virt *virt);

LSDN_DECLARE_ATTR(virt, mac, lsdn_mac_t);

/**
 * An entry in routing/forwarding table for a given virt. This may serve as a template for multiple
 * rules in different ruleset instances.
 *
 * This is preliminary, may change.
 */
struct lsdn_virt_rule{
	/* list in lsdn_virt */
	struct lsdn_list_entry virt_rules_entry;
	/* entries in lsdn_rule */
	struct lsdn_list_entry rule_instance_list;

	struct lsdn_virt *owning_virt;
	struct lsdn_match match;
};

lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);

#endif
