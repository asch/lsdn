/** \file
 * Main LSDN header file.
 * Contains definitions of structs and enums, and most of the API functions.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nettypes.h"

#define LSDN_DECLARE_ATTR(obj, name, type) \
	lsdn_err_t lsdn_##obj##_set_##name(struct lsdn_##obj *obj, type value); \
	lsdn_err_t lsdn_##obj##_clear_##name(struct lsdn_##obj *obj); \
	type lsdn_##obj##_get_##name(struct lsdn_##obj *obj)

struct lsdn_virt;
struct lsdn_net;
struct lsdn_phys;

typedef void (*lsdn_nomem_cb)(void *user);

struct lsdn_user_hooks {
	void (*lsdn_startup_hook)(struct lsdn_net *net, struct lsdn_phys *phys, void *user);
	void *lsdn_startup_hook_user;
	void (*lsdn_shutdown_hook)(struct lsdn_net *net, struct lsdn_phys *phys, void *user);
	void *lsdn_shutdown_hook_user;
};

/**
 * Top-level context encompassing all network topology. This includes virtual networks
 * (lsdn_network) and physical host connections (lsdn_phys). Only one context will typically exist
 * in a given program.
 *
 * The same structures (lsdn_phys, lsdn_virt) are used to describe both remote objects
 * and objects running on other machines. This allows the orchestrator to make the same API calls
 * on all physical machines to construct the network topology. The only difference between the
 * API calls on the physical machines will be the lsdn_phys_claim_local calls.
 */
struct lsdn_context;

struct lsdn_context *lsdn_context_new(const char* name);
void lsdn_context_set_nomem_callback(struct lsdn_context *ctx, lsdn_nomem_cb cb, void *user);
void lsdn_context_abort_on_nomem(struct lsdn_context *ctx);
void lsdn_context_free(struct lsdn_context *ctx);
/* Will automatically delete all child objects */
void lsdn_context_cleanup(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);

/** Type of network encapsulation. */
enum lsdn_nettype{
	/** VxLAN encapsulation. */
	LSDN_NET_VXLAN,
	/** VLAN encapsulation. */
	LSDN_NET_VLAN,
	/** No encapsulation. */
	LSDN_NET_DIRECT
};

/** Switch type for the virtual network. */
enum lsdn_switch{
	/** Learning switch with single tunnel shared from the phys.
	 * The network is essentially autoconfiguring in this mode.
	 */
	LSDN_LEARNING,
	/** Learning switch with a tunnel for each connected endpoint.
	 * In this mode the connection information (IP addr) for each physical node is required.
	 */
	LSDN_LEARNING_E2E,
	/** Static switching with a tunnel for each connected endpoint.
	 * In this mode we need the connection information + MAC addresses of all virts and where
	 * they reside.
	 *
	 * @note the endpoint is represented by a single linux interface,
	 * with the actual endpoint being selected by tc actions.
	 */
	LSDN_STATIC_E2E

	/* LSDN_STATIC does not exists, because it does not make much sense ATM. It would have
	 * static rules for the switching at local level, but it would go out through a single
	 * interface to be switched by some sort of learning switch. May be added if it appears.
	 */
};

/** Configuration structure for `lsdn_net`.
 * Multiple networks can share the same settings (e.g. vxlan with static routing on port 1234)
 * and only differ by their identifier (vlan id, vni ...).
 */
struct lsdn_settings;

struct lsdn_settings *lsdn_settings_new_direct(struct lsdn_context *ctx);
struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx);
struct lsdn_settings *lsdn_settings_new_vxlan_mcast(struct lsdn_context *ctx, lsdn_ip_t mcast_ip, uint16_t port);
struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port);
struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port);
void lsdn_settings_free(struct lsdn_settings *settings);
void lsdn_settings_register_user_hooks(struct lsdn_settings *settings, struct lsdn_user_hooks *user_hooks);
lsdn_err_t lsdn_settings_set_name(struct lsdn_settings *s, const char *name);
const char* lsdn_settings_get_name(struct lsdn_settings *s);
struct lsdn_settings *lsdn_settings_by_name(struct lsdn_context *ctx, const char *name);


/** Virtual network representation.
 * Nodes defined by `lsdn_virt` connect to a `lsdn_net` through physical host connections `lsdn_phys`.
 * Can be implemented using common tunneling techniques, like vlan or vxlan or no tunneling.
 *
 * Networks are defined by two main characteristics:
 *  - the tunnel used to overlay the network over physical topology (transparent to end users)
 *  - the switching methods used (visible to end users)
 */
struct lsdn_net;

struct lsdn_net *lsdn_net_new(struct lsdn_settings *settings, enum lsdn_ipv ipv, uint32_t vnet_id);
lsdn_err_t lsdn_net_set_name(struct lsdn_net *net, const char *name);
const char* lsdn_net_get_name(struct lsdn_net *net);
struct lsdn_net* lsdn_net_by_name(struct lsdn_context *ctx, const char *name);
/* Will automatically delete all child objects */
void lsdn_net_free(struct lsdn_net *net);

/** Physical host connection representation.
 * Represents a kernel interface for a node, e.g., eth0 on lsdn1.
 * Physical interfaces are used to connect to virtual networks. This connection is called
 * `lsdn_phys_attachement`.
 */
struct lsdn_phys;

struct lsdn_phys *lsdn_phys_new(struct lsdn_context *ctx);
lsdn_err_t lsdn_phys_set_name(struct lsdn_phys *phys, const char *name);
const char* lsdn_phys_get_name(struct lsdn_phys *phys);
struct lsdn_phys* lsdn_phys_by_name(struct lsdn_context *ctx, const char *name);
/* Will detach all virts */
void lsdn_phys_free(struct lsdn_phys *phys);
lsdn_err_t lsdn_phys_attach(struct lsdn_phys *phys, struct lsdn_net* net);
void lsdn_phys_detach(struct lsdn_phys *phys, struct lsdn_net* net);
lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys);
lsdn_err_t lsdn_phys_unclaim_local(struct lsdn_phys *phys);

LSDN_DECLARE_ATTR(phys, ip, lsdn_ip_t);
LSDN_DECLARE_ATTR(phys, iface, const char*);

/** Physical interface attachment.
 * A point of connection to a virtual network through a physical interface.
 * Only a single attachment may exist for a pair of `lsdn_phys` and `lsdn_net`.
 */
struct lsdn_phys_attachment;


/** Node in the virtual network.
 * A virtual machine (typically -- it may be any linux interface).
 *
 * Virtual machines participate in virtual networks (through phys_attachments on the host machine
 * connection). They can be migrated between the physical machines by connecting them through
 * different lsdn_phys.
 */
struct lsdn_virt;

struct lsdn_virt *lsdn_virt_new(struct lsdn_net *net);
void lsdn_virt_free(struct lsdn_virt* vsirt);
lsdn_err_t lsdn_virt_set_name(struct lsdn_virt *virt, const char *name);
const char* lsdn_virt_get_name(struct lsdn_virt *virt);
struct lsdn_virt* lsdn_virt_by_name(struct lsdn_net *net, const char *name);
lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt *virt, struct lsdn_phys *phys, const char *iface);
void lsdn_virt_disconnect(struct lsdn_virt *virt);

LSDN_DECLARE_ATTR(virt, mac, lsdn_mac_t);

lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
