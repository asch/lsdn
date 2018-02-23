/** \file
 * Error reporting related structs and definitions.
 */
#pragma once

#include "util.h"

#include <stddef.h>
#include <stdio.h>

struct lsdn_context;

/** Possible LSDN errors. */
typedef enum {
	/** No error. */
	LSDNE_OK = 0,
	/** Out of memory. */
	LSDNE_NOMEM,
	/** Parsing from string has failed.
	 * Can occur when parsing IPs, MACs etc. */
	LSDNE_PARSE,
	/** Duplicate name.
	 * Can occur when setting name for a network, virt or phys. */
	LSDNE_DUPLICATE,
	/** Interface does not exist. */
	LSDNE_NOIF,
	/** Netlink error. */
	LSDNE_NETLINK,
	/** Operating system error. */
	LSDNE_OS,
	/** Incompatible rules with the same priority */
	LSDNE_INCOMPATIBLE_MATCH,
	/** Network model validation failed, and the old model is in effect. */
	LSDNE_VALIDATE,
	/** Network model commit failed and a mix of old, new and dysfunctional objects are in effect.
	 *
	 * If object state is #LSDN_OK, the new model is in effect for that object, if it is #LSDN_RENEW
	 * or #LSDN_NEW, the old model is in effect.
	 *
	 * The object can also get to #LSDN_NEW state during updating, if the updating failed. This
	 * makes the object behave as if it did not exist.
	 *
	 * In either case, you can retry the commit and it will work if the error was temporary.
	 *
	 * The error could also be permanent, if, for example, a user have created a network interface
	 * that shares a name with what LSDN was going to use. In that case, you will be getting the
	 * error repeatedly. You can either ignore it or delete the failing part of the model. */
	LSDNE_COMMIT,
	/** Cleanup operation has failed and this left an object in state inconsistent with the model.
	 *
	 * This failure is more serious than #LSDNE_COMMIT failure, since the commit operation can
	 * not be successfully retried. The only operation possible is to rebuild the whole model again. */
	LSDNE_INCONSISTENT,
} lsdn_err_t;

/** Generator for #lsdn_problem_code.
 * @see LSDN_ENUM
 * @see lsdn_problem_code */
#define lsdn_enumgen_problem_code(x) \
	/** Missing attribute on a phys. */ \
	x(LSDNP_PHYS_NOATTR, "An attribute %o must be defined on phys %o for attachment to net %o.") \
	/** Duplicate attribute on two phys's in the same network. */ \
	x(LSDNP_PHYS_DUPATTR, "Duplicate attribute %o specified for phys %o and phys %o.") \
	/** Incompatible IP versions in the same network. */ \
	x(LSDNP_PHYS_INCOMPATIBLE_IPV, "Phys %o and phys %o attached to net %o have incompatible ip versions.") \
	/** Connecting a virt from a phys that is not attached to a network. */ \
	x(LSDNP_PHYS_NOT_ATTACHED, "Trying to connect virt %o to a network %o on phys %o, but the phys is not attached to that network.") \
	/** Interface specified for a virt does not exist. */ \
	x(LSDNP_VIRT_NOIF, "The interface %o specified for virt %o does not exist.") \
	/** Missing attribute on a virt. */ \
	x(LSDNP_VIRT_NOATTR, "An attribute %o must be defined on virt %o connected to net %o.") \
	/** Duplicate attribute on two virts in the same network. */ \
	x(LSDNP_VIRT_DUPATTR, "Duplicate attribute %o specified for virt %o and virt %o connected to net %o.") \
	/** Incompatible networks on the same machine. */ \
	x(LSDNP_NET_BAD_NETTYPE, "Trying to create net %o and net %o of incompatible network types on the same machine.") \
	/** Bad network ID. */ \
	x(LSDNP_NET_BADID, "Trying to create net %o with net id %o that is out of the allowed range for this type of network.") \
	/** Duplicate network ID. */ \
	x(LSDNP_NET_DUPID, "Trying to create net %o and net %o with the same net id %o.") \
	/** Two incompatible virt rules with the same priority. */ \
	x(LSDNP_VR_INCOMPATIBLE_MATCH, "Rules %o and %o on virt %o share the same priority, but have different match targets or masks.")\
	/** Duplicate virt rules. */ \
	x(LSDNP_VR_DUPLICATE_RULE, "Rules %o and %o on virt %o share the same priority and are completely equal") \
	/** Committing to netlink failed due to kernel error. */ \
	x(LSDNP_COMMIT_NETLINK, "Committing %o failed because kernel has refused the operation") \
	/** Decommitting to netlink failed due to kernel error and the state is now inconsistent. */ \
	x(LSDNP_COMMIT_NETLINK_CLEANUP, "Cleanup of %o failed because kernel has refused the operation. It has been left in inconsistent state.") \
	/** Committing to netlink failed due to memory error. */ \
	x(LSDNP_COMMIT_NOMEM, "Committing %o failed because memory was exhausted.") \
	/** Can not establish netlink communication */ \
	x(LSDNP_NO_NLSOCK, "Can not establish netlink socket.") \
	/** QoS has either zero rate or zero burst. See #lsdn_qos_rate_t for correct parameters. */ \
	x(LSDNP_RATES_INVALID, "Effective QoS (%o) rate on %o is zero. This is probably not the rate you are looking for.")

/** Validation and commit errors. */
LSDN_ENUM(problem_code, LSDNP);

/** Problem reference type. */
enum lsdn_problem_ref_type {
	/** Problem with attribute. */
	LSDNS_ATTR,
	/** Problem with #lsdn_phys. */
	LSDNS_PHYS,
	/** Problem with #lsdn_net and #lsdn_phys combination. */
	LSDNS_PA,
	/** Problem with #lsdn_net. */
	LSDNS_NET,
	/** Problem with #lsdn_virt. */
	LSDNS_VIRT,
	/** Problem with #lsdn_if. */
	LSDNS_IF,
	/** Problem with `vnet_id`. */
	LSDNS_NETID,
	/** Problem with #lsdn_vr. */
	LSDNS_VR,
	/** End of problem list. */
	LSDNS_END
};

/** Maximum number of problem items described simultaneously.
 * Configures size of the problem buffer in #lsdn_context. */
#define LSDN_MAX_PROBLEM_REFS 10

/** Reference to a problem item.
 * Consists of a problem type, and a pointer to a struct of the appropriate type. */
struct lsdn_problem_ref {
	/** Problem type. */
	enum lsdn_problem_ref_type type;
	/** Pointer to the appropriate struct.
	 * TODO this should probably be a union? */
	void *ptr;
};

/** Description of encountered problem.
 * Passed to a #lsdn_problem_cb callback when an error occurs.
 *
 * `code` refers to the type of problem encountered. Depending on the type of the problem,
 * this might also indicate any number of related problematic items. Pointers to them
 * are stored in `refs`. */
struct lsdn_problem {
	/** Problem code. */
	enum lsdn_problem_code code;
	/** Number of related items. */
	size_t refs_count;
	/** Array of references to related items.
	 * @note `refs` actually point to a buffer in #lsdn_context. */
	struct lsdn_problem_ref *refs;
};

void lsdn_problem_format(FILE* out, const struct lsdn_problem *problem);
void lsdn_problem_report(struct lsdn_context *ctx, enum lsdn_problem_code code, ...);

/** Problem handler callback.
 * @param diag description of the problem.
 * @param user user-specified data. */
typedef void (*lsdn_problem_cb)(const struct lsdn_problem *diag, void *user);
void lsdn_problem_stderr_handler(const struct lsdn_problem *problem, void *user);
