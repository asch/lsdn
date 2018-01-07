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
	/** Network model validation failed. */
	LSDNE_VALIDATE,
	/** Network model commit failed. */
	LSDNE_COMMIT,
	/** Incompatible rules with the same priority */
	LSDNE_INCOMPATIBLE_MATCH
} lsdn_err_t;

/** Generator for `lsdn_problem_code`.
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
	/** Duplicate network ID. */ \
	x(LSDNP_NET_DUPID, "Trying to create net %o and net %o with the same net id %o.") \
	/** Two incompatible virt rules with the same priority. */ \
	x(LSDNP_VR_INCOMPATIBLE_MATCH, "Rules %o and %o on virt %o share the same priority, but have different match targets or masks.")\
	/** Duplicate virt rules. */ \
	x(LSDNP_VR_DUPLICATE_RULE, "Rules %o and %o on virt %o share the same priority and are completely equal")

/** Validation and commit errors. */
LSDN_ENUM(problem_code, LSDNP);

/** Problem reference type. */
enum lsdn_problem_ref_type {
	/** Problem with attribute. */
	LSDNS_ATTR,
	/** Problem with `lsdn_phys`. */
	LSDNS_PHYS,
	/** Problem with `lsdn_net`. */
	LSDNS_NET,
	/** Problem with `lsdn_virt`. */
	LSDNS_VIRT,
	/** Problem with `lsdn_if`. */
	LSDNS_IF,
	/** Problem with `vnet_id`. */
	LSDNS_NETID,
	/** Problem with `lsdn_vr`. */
	LSDNS_VR,
	/** End of problem list. */
	LSDNS_END
};

#define LSDN_MAX_PROBLEM_REFS 10

/** Single problem reference. */
struct lsdn_problem_ref {
	/** Problem type. */
	enum lsdn_problem_ref_type type;
	/** Associated data, depending on problem type.
	 * TODO this should probably be a union. */
	void *ptr;
};

/** Something about a problem. XXX */
struct lsdn_problem {
	enum lsdn_problem_code code;
	size_t refs_count;
	struct lsdn_problem_ref *refs;
};

void lsdn_problem_format(FILE* out, const struct lsdn_problem *problem);
void lsdn_problem_report(struct lsdn_context *ctx, enum lsdn_problem_code code, ...);

/** Problem handler callback.
 * @param diag description of the problem.
 * @param user user-specified data. */
typedef void (*lsdn_problem_cb)(const struct lsdn_problem *diag, void *user);
void lsdn_problem_stderr_handler(const struct lsdn_problem *problem, void *user);
