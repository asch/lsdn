/** \file
 * Main LSDN header file.
 * Contains definitions of structs and enums, and most of the API functions. */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "nettypes.h"

/** Attribute generator.
 * @private
 * Declares a setter and a "clearer" functions for attribute `attr` of type `type_in`.
 * This is used to simplify creating accessors for attributes.
 * @note Input and output types are specified manully. This is to prevent problems
 * with value types that are already pointers or consts (like `const char *`).
 * @param desc human name for the attribute. Used in generated docs.
 * @param obj type on which the attribute is declared
 * @param attr name of the attribute field
 * @param type_in type of the attribute field (when set)
 * @param type_out type of the attribute field (when read)
 */
#define LSDN_DECLARE_ATTR(desc, obj, attr, type_in, type_out) \
	/** Set desc of a obj.
@param object obj to modify.
@param value desc. */ \
	lsdn_err_t lsdn_##obj##_set_##attr(struct lsdn_##obj *object, type_in value); \
	/** Get desc of a obj.
The pointer is valid until the attribute is changed or object freed.
@param object obj to query.
@return value of desc attribute, or `NULL` if unset. */ \
	type_out lsdn_##obj##_get_##attr(struct lsdn_##obj *object); \
	/** Clear desc of a obj.
@param object obj to modify. */ \
	void lsdn_##obj##_clear_##attr(struct lsdn_##obj *object)

/** LSDN Context.
 * @struct lsdn_context
 * @ingroup context
 * The base object of the LSDN network model. There should be exactly one instance in your program.
 * @rstref{capi/context} */
struct lsdn_context;

/** Virtual network.
 * @struct lsdn_net
 * @ingroup network
 * Network is a collection of virts that can communicate with each other.
 * @rstref{capi/network}
 * @rstref{capi/virt}
 * @rstref{capi/phys} */
struct lsdn_net;

/** Virt.
 * @struct lsdn_virt
 * @ingroup virt
 * A virtual machine (typically -- it may be any Linux interface).
 *
 * Virts are tenants in networks. They must be connected through a phys. They can be migrated between
 * physes at runtime.
 * @rstref{capi/virt}
 * @rstref{capi/phys}
 * @rstref{capi/network} */
struct lsdn_virt;

/** Phys.
 * @struct lsdn_phys
 * @ingroup phys
 * Represents a kernel interface for a host node, e.g., eth0 on lsdn1.
 * Physes are attached to network, and then virts can connect through them.
 * @rstref{capi/phys}
 * @rstref{capi/virt}
 * @rstref{capi/network} */
struct lsdn_phys;

/** Configuration structure for a virtual network.
 * @struct lsdn_settings
 * @ingroup network
 * Multiple networks can share the same settings (e.g. VXLAN with static routing on port 1234)
 * and only differ by their identifier (VLAN id, VNI...).
 * @rstref{capi/network} */
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
 * Configured as part of #lsdn_settings, this structure holds the callback hooks for startup
 * and shutdown, and their custom data. */
struct lsdn_user_hooks {
	/** Startup hook.
	 * Called at commit time for every local phys and every network to which it is attached.
	 * @param net network.
	 * @param phys attached phys.
	 * @param user receives the value of #lsdn_startup_hook_user. */
	void (*lsdn_startup_hook)(struct lsdn_net *net, struct lsdn_phys *phys, void *user);
	/** Custom value for #lsdn_startup_hook. */
	void *lsdn_startup_hook_user;
	/** Shutdown hook.
	 * Currently unused. TODO. */
	void (*lsdn_shutdown_hook)(struct lsdn_net *net, struct lsdn_phys *phys, void *user);
	/** Custom value for #lsdn_shutdown_hook. */
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
/** Ignore if any of the interfaces or rules LSDN would create already exists.
 *
 * This flags is active by default. It ensures that LSDN will ignore rules created by previous
 * crashed instances.
 */
void lsdn_context_set_overwrite(struct lsdn_context *ctx, bool overwrite);
/** Query if LSDN should overwrite any of the interfaces or rules.
 * @see lsdn_context_set_overwrite
 */
bool lsdn_context_get_overwrite(struct lsdn_context *ctx);

lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user);
/** @} */


/** @defgroup network Virtual network
 * Functions, and related data types, for manipulating network objects and their settings.
 *
 * Virtual network is a collection of virts that can communicate with each other
 * as if they were on the same LAN. At the same time, they are isolated from
 * other virtual networks, as well as from the host network. Distinct virtual
 * networks can have hosts with same MAC addresses, and it is impossible to read
 * packets belonging to other networks (or the host network), or send packets
 * that travel outside the virtual network.
 *
 * The #lsdn_net object represents a network in the sense of "collection of
 * virts". Apart from basic life-cycle and lookup functions, it is only possible
 * to add or remove virts to/from it.
 *
 * Configuration of network properties is done through separate #lsdn_settings
 * objects. There is a `lsdn_<kind>_settings_new` function for each kind of
 * network encapsulation, with different required parameters. It is also
 * possible to register _user hooks_ for startup and shutdown events.
 *
 * An exception to this is the `vnet_id` property, which is set on a network
 * directly, as opposed to being a part of settings. It configures the VNET
 * (or encapsulation ID) of the network. That means that several networks can
 * share a common settings object while still being differentiated by `vnet_id`. */


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

LSDN_DECLARE_ATTR(IP address, phys, ip, lsdn_ip_t, const lsdn_ip_t*);
LSDN_DECLARE_ATTR(interface, phys, iface, const char*, const char*);
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
/** Get a recommended MTU for a given virt.
 * The MTU is based on the current state and connection port of the virt (it is not based on the
 * committed stated). The phys interface must already exist. */
lsdn_err_t lsdn_virt_get_recommended_mtu(struct lsdn_virt *virt, unsigned int *mtu);

/** Bandwidth limit for virt's interface (for one direction).
 * @ingroup virt
 * @see lsdn_virt_set_rate_out
 * @see lsdn_virt_set_rate_in*/
typedef struct {
	/**
	 * Bandwidth restriction in bytes per second.
	 */
	float avg_rate;
	/**
	 * A size of data burst that is allowed to exceed the #avg_rate.
	 *
	 * It is not possible to leave this field zero, because no packets would go through. Since
	 * each packet is considered as a short burst, the burst rate must be at least as big as
	 * your MTU.
	 */
	uint32_t burst_size;
	/**
	 * An absolute restriction on the bandwidth in bytes per second, even during bursting.
	 *
	 * If zero is given, the peak rate is unrestricted.
	 */
	float burst_rate;
} lsdn_qos_rate_t;

LSDN_DECLARE_ATTR(MAC address, virt, mac, lsdn_mac_t, const lsdn_mac_t*);
LSDN_DECLARE_ATTR(inbound bandwidth limit, virt, rate_in, lsdn_qos_rate_t, const lsdn_qos_rate_t*);
LSDN_DECLARE_ATTR(outbound bandwidth limit, virt, rate_out, lsdn_qos_rate_t, const lsdn_qos_rate_t*);
/** @} */

/** @defgroup misc Miscellaneous
 * TODO describe what you can find here. For example, dump functions. */

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
