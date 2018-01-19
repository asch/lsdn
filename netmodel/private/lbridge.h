/** \file
 * Linux bridge related structs and definitions. */
#pragma once

#include "nl.h"

struct lsdn_virt;
struct lsdn_phys_attachment;

/** Linux Bridge.
 * Currently only holds a reference to the context
 * and to the bridge interface. */
struct lsdn_lbridge {
	/** LSDN context */
	struct lsdn_context *ctx;
	/** Underlying bridge interface */
	struct lsdn_if bridge_if;
};

/** Linux Bridge interface.
 * Currently only holds a reference to the `lsdn_lbridge` structure
 * and to the connected interface. */
struct lsdn_lbridge_if {
	/** LSDN Linux Bridge */
	struct lsdn_lbridge *br;
	/** Connected interface. */
	struct lsdn_if *iface;
};

lsdn_err_t lsdn_lbridge_init(struct lsdn_context *ctx, struct lsdn_lbridge *br);
lsdn_err_t lsdn_lbridge_free(struct lsdn_lbridge *br);
lsdn_err_t lsdn_lbridge_add(struct lsdn_lbridge *br, struct lsdn_lbridge_if *br_if, struct lsdn_if *iface);
lsdn_err_t lsdn_lbridge_remove(struct lsdn_lbridge_if *iface);

/* lsdn_net_ops implementations and helpers */
lsdn_err_t lsdn_lbridge_create_pa(struct lsdn_phys_attachment *a);
lsdn_err_t lsdn_lbridge_destroy_pa(struct lsdn_phys_attachment *a);
lsdn_err_t lsdn_lbridge_add_virt(struct lsdn_virt *v);
lsdn_err_t lsdn_lbridge_remove_virt(struct lsdn_virt *v);
