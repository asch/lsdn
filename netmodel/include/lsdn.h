/** \file
 * Main LSDN header file.
 * Contains definitions of structs and enums, and most of the API functions. */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nettypes.h"

/** Attribute generator.
 * @private
 * Declares a setter and a "clearer" functions for attribute `attr` of type `type.
 * This is used to simplify creating accessors for attributes.
 * @note getter function must be declared manually. This is to prevent problems
 * with value types that are already pointers or consts (like `const char *`).
 * @param desc human name for the attribute. Used in generated docs.
 * @param obj type on which the attribute is declared
 * @param attr name of the attribute field
 * @param type type of the attribute field */
#define LSDN_DECLARE_ATTR(desc, obj, attr, type) \
	/** Set desc of a obj.
@param obj obj to modify.
@param value desc. */ \
	lsdn_err_t lsdn_##obj##_set_##attr(struct lsdn_##obj *obj, type value); \
	/** Clear desc of a obj.
@param obj obj to modify. */ \
	lsdn_err_t lsdn_##obj##_clear_##attr(struct lsdn_##obj *obj)

/* XXX the following struct documentation snippets are NOT USED.
 * I'm not sure why. Instead, descriptions in private/lsdn.h are used,
 * and these structs are not even listed in the public header documentation.
 * What do.? */

/** Context.
 * This is documented elsewhere. */
struct lsdn_context;

/** Virtual network representation.
 * Nodes defined by `lsdn_virt` connect to a `lsdn_net` through physical host connections `lsdn_phys`.
 * Can be implemented using common tunneling techniques, like vlan or vxlan or no tunneling.
 *
 * Networks are defined by two main characteristics:
 *  - the tunnel used to overlay the network over physical topology (transparent to end users)
 *  - the switching methods used (visible to end users) */
struct lsdn_net;

/** Node in the virtual network.
 * A virtual machine (typically -- it may be any linux interface).
 *
 * Virtual machines participate in virtual networks (through phys_attachments on the host machine
 * connection). They can be migrated between the physical machines by connecting them through
 * different `lsdn_phys`. */
struct lsdn_virt;

/** Physical host connection representation.
 * Represents a kernel interface for a node, e.g., eth0 on lsdn1.
 * Physical interfaces are used to connect to virtual networks. This connection is called
 * `lsdn_phys_attachement`. */
struct lsdn_phys;

/** Physical interface attachment.
 * A point of connection to a virtual network through a physical interface.
 * Only a single attachment may exist for a pair of `lsdn_phys` and `lsdn_net`. */
struct lsdn_phys_attachment;

/** Configuration structure for `lsdn_net`.
 * Multiple networks can share the same settings (e.g. vxlan with static routing on port 1234)
 * and only differ by their identifier (vlan id, vni ...). */
struct lsdn_settings;

/** Virt rule direction.
 * @ingroup rules */
enum lsdn_direction {
	/** Inbound rule. */
	LSDN_IN,
	/** Outbound rule. */
	LSDN_OUT
};

/** Signature for out-of-memory callback.
 * @ingroup context */
typedef void (*lsdn_nomem_cb)(void *user);

/** User callback hooks.
 * @ingroup network
 * Configured as part of `lsdn_settings`, this structure holds the callback hooks for startup
 * and shutdown, and their custom data. */
struct lsdn_user_hooks {
	/** Startup hook.
	 * Called at commit time for every local phys and every network it's attached to.
	 * @param net network.
	 * @param phys attached phys.
	 * @param user receives the value of `lsdn_startup_hook_user`. */
	void (*lsdn_startup_hook)(struct lsdn_net *net, struct lsdn_phys *phys, void *user);
	/** Custom value for `lsdn_startup_hook`. */
	void *lsdn_startup_hook_user;
	/** Shutdown hook.
	 * Currently unused. TODO. */
	void (*lsdn_shutdown_hook)(struct lsdn_net *net, struct lsdn_phys *phys, void *user);
	/** Custom value for `lsdn_shutdown_hook`. */
	void *lsdn_shutdown_hook_user;
};

/** @defgroup context Context
 * Context, commits and high level network model management.
 *
 * LSDN context is a core object that manages the network model.
 * It allows the app to keep track of constraints (such as unique names, no two virts using
 * the same interface, etc.), validate the model and commit it to kernel tables.
 *
 * Context also keeps track of all the child objects (settings, networks, virts, physes, names,
 * etc.) and automatically frees them when it is deleted through #lsdn_context_free or
 * #lsdn_context_cleanup.
 *
 * In practically every conceivable case, a single app should only have one context; in fact,
 * only one context should exist per physical host. The library allows you to have multiple contexts
 * at the same time (which is equivalent to having multiple instances of an app), but in such case,
 * the user is responsible for conflicting rules on interfaces.
 * In other words: don't do this, things will probably crash and burn if you do.
 * @{ */
struct lsdn_context *lsdn_context_new(const char* name);
void lsdn_context_set_nomem_callback(struct lsdn_context *ctx, lsdn_nomem_cb cb, void *user);
void lsdn_context_abort_on_nomem(struct lsdn_context *ctx);
void lsdn_context_free(struct lsdn_context *ctx);
/* Will automatically delete all child objects */
void lsdn_context_cleanup(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);

lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
/** @} */

/** Type of network encapsulation.
 * @ingroup network */
enum lsdn_nettype {
	/** VXLAN encapsulation. */
	LSDN_NET_VXLAN,
	/** VLAN encapsulation. */
	LSDN_NET_VLAN,
	/** No encapsulation. */
	LSDN_NET_DIRECT,
	/** GENEVE enacpsulation */
	LSDN_NET_GENEVE
};

/** Switch type for the virtual network.
 * @ingroup network */
enum lsdn_switch {
	/** Learning switch with single tunnel shared from the phys.
	 * The network is essentially autoconfiguring in this mode. */
	LSDN_LEARNING,
	/** Learning switch with a tunnel for each connected endpoint.
	 * In this mode the connection information (IP addr) for each physical node is required. */
	LSDN_LEARNING_E2E,
	/** Static switching with a tunnel for each connected endpoint.
	 * In this mode we need the connection information + MAC addresses of all virts and where
	 * they reside.
	 *
	 * @note the endpoint is represented by a single linux interface,
	 * with the actual endpoint being selected by tc actions. */
	LSDN_STATIC_E2E

	/* LSDN_STATIC does not exists, because it does not make much sense ATM. It would have
	 * static rules for the switching at local level, but it would go out through a single
	 * interface to be switched by some sort of learning switch. May be added if it appears. */
};

/** @defgroup network Virtual network */

/** @name Network object management
 * @{ */
/** @ingroup network */
struct lsdn_net *lsdn_net_new(struct lsdn_settings *settings, uint32_t vnet_id);
lsdn_err_t lsdn_net_set_name(struct lsdn_net *net, const char *name);
const char* lsdn_net_get_name(struct lsdn_net *net);
struct lsdn_net* lsdn_net_by_name(struct lsdn_context *ctx, const char *name);
/* Will automatically delete all child objects */
void lsdn_net_free(struct lsdn_net *net);
/** @}*/

/** @name Network settings
 * @{ */
/** @ingroup network */
struct lsdn_settings *lsdn_settings_new_direct(struct lsdn_context *ctx);
struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx);
struct lsdn_settings *lsdn_settings_new_vxlan_mcast(struct lsdn_context *ctx, lsdn_ip_t mcast_ip, uint16_t port);
struct lsdn_settings *lsdn_settings_new_vxlan_e2e(struct lsdn_context *ctx, uint16_t port);
struct lsdn_settings *lsdn_settings_new_vxlan_static(struct lsdn_context *ctx, uint16_t port);
struct lsdn_settings *lsdn_settings_new_geneve(struct lsdn_context *ctx, uint16_t port);
void lsdn_settings_free(struct lsdn_settings *settings);
void lsdn_settings_register_user_hooks(struct lsdn_settings *settings, struct lsdn_user_hooks *user_hooks);
lsdn_err_t lsdn_settings_set_name(struct lsdn_settings *s, const char *name);
const char* lsdn_settings_get_name(struct lsdn_settings *s);
struct lsdn_settings *lsdn_settings_by_name(struct lsdn_context *ctx, const char *name);
/** @} */

/** @defgroup phys Phys (host machine)
 * Functions for manipulating and configuring phys objects.
 *
 * Phys is a representation of a physical machine that hosts tenants of virtual
 * networks.  Its `interface` attribute specifies the name of the network
 * interface that is connected to the host network. In addition, some network
 * types require IP addresses of physes.
 *
 * In order to start connecting virts, a phys must be _attached_ to a virtual
 * network. That only marks the phys as a participant in that network; a single
 * phys can be attached to any number of networks.
 *
 * In the network model, all physes must be represented on all machines. To
 * select the current machine and configure network viewpoint, you must call
 * #lsdn_phys_claim_local. Kernel rules are then generated from the viewpoint of
 * that phys.
 *
 * It is possible to have multiple physes on the same machine and claimed local.
 * This is useful in situations where the host machine has more than one
 * interface connecting to a host network, or if the machine connects to more
 * than one host network.
 * @{ */
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

LSDN_DECLARE_ATTR(IP address, phys, ip, lsdn_ip_t);
LSDN_DECLARE_ATTR(interface, phys, iface, const char*);
/** @} */


/** @defgroup virt Virt management
 * @{ */
struct lsdn_virt *lsdn_virt_new(struct lsdn_net *net);
void lsdn_virt_free(struct lsdn_virt* vsirt);
struct lsdn_net *lsdn_virt_get_net(struct lsdn_virt *virt);
lsdn_err_t lsdn_virt_set_name(struct lsdn_virt *virt, const char *name);
const char* lsdn_virt_get_name(struct lsdn_virt *virt);
struct lsdn_virt* lsdn_virt_by_name(struct lsdn_net *net, const char *name);
lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt *virt, struct lsdn_phys *phys, const char *iface);
void lsdn_virt_disconnect(struct lsdn_virt *virt);
/** Get a recommanded MTU for a given virt.
 * The MTU is based on the current state and connection port of the virt (it is not based on the
 * committed stated). The phys interface must already exist. */
lsdn_err_t lsdn_virt_get_recommended_mtu(struct lsdn_virt *virt, unsigned int *mtu);

/** Bandwidth limit for virt's interface (for one direction).
 * @ingroup virt
 * @see lsdn_set_qos_rate */
typedef struct {
	/* in bytes per second */
	float avg_rate;
	/* in bytes */
	uint32_t burst_size;
	/* in bytes per second */
	float burst_rate;
} lsdn_qos_rate_t;

LSDN_DECLARE_ATTR(MAC address, virt, mac, lsdn_mac_t);
LSDN_DECLARE_ATTR(inbound bandwidth limit, virt, rate_in, lsdn_qos_rate_t);
LSDN_DECLARE_ATTR(outbound bandwidth limit, virt, rate_out, lsdn_qos_rate_t);
/** @} */

/** @defgroup misc Miscellaneous */

const char *lsdn_mk_name(struct lsdn_context *ctx, const char *type);

/** Generate unique name for a net.
 * @ingroup network
 * @param ctx LSDN context.
 * @see lsdn_mk_name */
#define lsdn_mk_net_name(ctx) lsdn_mk_name(ctx, "net")
/** Generate unique name for a phys.
 * @ingroup phys
 * @param ctx LSDN context.
 * @see lsdn_mk_name */
#define lsdn_mk_phys_name(ctx) lsdn_mk_name(ctx, "phys")
/** Generate unique name for a virt.
 * @ingroup virt
 * @param ctx LSDN context.
 * @see lsdn_mk_name */
#define lsdn_mk_virt_name(ctx) lsdn_mk_name(ctx, "virt")
/** Generate unique name for an interface.
 * @ingroup misc
 * @param ctx LSDN context.
 * @see lsdn_mk_name */
#define lsdn_mk_iface_name(ctx) lsdn_mk_name(ctx, "iface")
/** Generate unique name for a settings object.
 * @ingroup network
 * @param ctx LSDN context.
 * @see lsdn_mk_name */
#define lsdn_mk_settings_name(ctx) lsdn_mk_name(ctx, "settings")
