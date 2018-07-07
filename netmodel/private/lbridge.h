/** \file
 * Linux bridge related structs and definitions. */
#pragma once

#include "nl.h"
#include "rules.h"
#include "clist.h"
#include "sbridge.h"

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

/** A dummy interface that acts as a front-end to a light-weight meta-data tunnel */
struct lsdn_lbridge_route {
	/* Callback to add an action setting the tunnel metadata.*/
	struct lsdn_action_desc tunnel_action;
	struct lsdn_sbridge_phys_if *phys_if;
	enum lsdn_rule_target vid_match;
	union lsdn_matchdata vid_matchdata;
	enum lsdn_rule_target source_match;
	union lsdn_matchdata source_matchdata;

	/* Private part starts here */
	struct lsdn_if dummy_if;
	/* Ruleset for the "out" direction -- lbridge sending packets to the tunnel */
	struct lsdn_ruleset ruleset;
	struct lsdn_ruleset_prio *out_redir;
	struct lsdn_ruleset_prio *out_filter;
	struct lsdn_rule out_redir_rule;
	struct lsdn_rule out_filter_rule;
	struct lsdn_rule in_redir_rule;
	struct lsdn_lbridge_if lbridge_if;
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
lsdn_err_t lsdn_lbridge_add_route(struct lsdn_lbridge *br, struct lsdn_lbridge_route *route, uint32_t vid);
lsdn_err_t lsdn_lbridge_remove_route(struct lsdn_lbridge_route *route);
