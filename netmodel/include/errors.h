/** \file
 * Error reporting related structs and definitions.
 */
#pragma once

#include <stddef.h>
#include <stdio.h>

struct lsdn_context;

/** Possible LSDN errors. */
enum lsdn_err {
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
};
typedef enum lsdn_err lsdn_err_t;

#define lsdn_foreach_problem(x) \
	x(PHYS_NOATTR, "An attribute %o must be defined on phys %o for attachment to net %o.") \
	x(PHYS_DUPATTR, "Duplicate attribute %o specified for phys %o and phys %o.") \
	x(PHYS_NOT_ATTACHED, "Trying to connect virt %o to a network %o on phys %o, but the phys is not attached to that network.") \
	x(VIRT_NOIF, "The interface %o specified for virt %o does not exist.") \
	x(VIRT_NOATTR, "An attribute %o must be defined on virt %o connected to net %o.") \
	x(VIRT_DUPATTR, "Duplicate attribute %o specified for virt %o and virt %o connected to net %o.") \
	x(NET_BAD_NETTYPE, "Trying to create net %o and net %o of incompatible network types on the same machine.") \
	x(NET_DUPID, "Trying to create net %o and net %o with the same net id %o.") \
	x(VR_INCOMPATIBLE_MATCH, "Rules %o and %o on virt %o share the same priority, but have different match targets or masks.")\
	x(VR_DUPLICATE_RULE, "Rules %o and %o on virt %o share the same priority and are completely equal")

#define lsdn_mk_problem_enum(name, string) LSDNP_##name,

/* Information about validation or commit error */
enum lsdn_problem_code {
	lsdn_foreach_problem(lsdn_mk_problem_enum)
};

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
