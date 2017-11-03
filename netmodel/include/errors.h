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
	/** Operating system error. */
	LSDNE_OS,
	/** Network model validation failed. */
	LSDNE_VALIDATE,
	/** Network model commit failed. */
	LSDNE_COMMIT,
};
typedef enum lsdn_err lsdn_err_t;

#define lsdn_foreach_problem(x) \
	x(PHYS_NOATTR, "An attribute %o must be defined on phys %o for attachment to net %o.") \
	x(PHYS_NOT_ATTACHED, "Trying to connect virt %o to a network %o on phys %o, but the phys is not attached to that network.") \
	x(VIRT_NOIF, "The interface %o specified for virt %o does not exist.")

#define lsdn_mk_problem_enum(name, string) LSDNP_##name,

/* Information about validation or commit error */
enum lsdn_problem_code {
	lsdn_foreach_problem(lsdn_mk_problem_enum)
};

enum lsdn_problem_ref_type {
	LSDNS_ATTR,
	LSDNS_PHYS,
	LSDNS_NET,
	LSDNS_VIRT,
	LSDNS_IF,
	LSDNS_END
};

#define LSDN_MAX_PROBLEM_REFS 10

struct lsdn_problem_ref {
	enum lsdn_problem_ref_type type;
	void *ptr;
};

struct lsdn_problem {
	enum lsdn_problem_code code;
	size_t refs_count;
	struct lsdn_problem_ref *refs;
};

void lsdn_problem_format(FILE* out, const struct lsdn_problem *problem);
/* The variadic arguments are pairs of subject type and pointers. The las type must be LSDNS_END */
void lsdn_problem_report(struct lsdn_context *ctx, enum lsdn_problem_code code, ...);

typedef void (*lsdn_problem_cb)(const struct lsdn_problem *diag, void *user);
void lsdn_problem_stderr_handler(const struct lsdn_problem *problem, void *user);
