/** \file
 * Virt Rules engine related private structs and definitions. */
#pragma once

#include <uthash.h>
#include "list.h"
#include <stdbool.h>
#include "nl.h"
#include "idalloc.h"
#include "../include/nettypes.h"
#include "../include/rules.h"
#include "list.h"
#include "state.h"

#define LSDN_IF_PRIO_POLICING 0xFF00
#define LSDN_IF_PRIO_MATCH 0xFF01
#define LSDN_IF_PRIO_FALLBACK 0xFF02
#define LSDN_IF_PRIO_SOURCE 0xFF03
#define LSDN_SBRIDGE_IF_SUBPRIO 0xFFFFFF00

/** Action generator callback signature.
 * @see lsdn_action_desc */
typedef void (*lsdn_mkaction_fn)(struct lsdn_filter *filter, uint16_t order, void *user);

/** Description of rule actions.
 * Describes a sequence of TC actions constructed by a callback when needed.
 * TODO expand on this */
struct lsdn_action_desc {
	/** Name. */
	char *name;
	/** Number of actions in sequence. */
	size_t actions_count;
	/** Callback that generates the actions. */
	lsdn_mkaction_fn fn;
	/** User data for the callback. */
	void *user;
};

void lsdn_action_init(struct lsdn_action_desc *action, size_t count, lsdn_mkaction_fn fn, void *user);
bool lsdn_target_supports_masking(enum lsdn_rule_target);

struct lsdn_flower_rule;

/** Maximum size of #lsdn_matchdata struct, in bytes.
 * This should be the same as `sizeof(union lsdn_matchdata)`. But this value
 * is used to declare size of #lsdn_matchdata.bytes field, so using `sizeof`
 * is not possible. */
#define LSDN_MAX_MATCH_LEN 16

/** Value to be matched. */
union lsdn_matchdata {
	/** Value as raw bytes. */
	char bytes[LSDN_MAX_MATCH_LEN];
	/** Value as MAC address. */
	lsdn_mac_t mac;
	/** Value as IPv4 address. */
	lsdn_ipv4_t ipv4;
	/** Value as IPv6 address. */
	lsdn_ipv6_t ipv6;
	/** Value as tunnel key ID. */
	uint32_t enc_key_id;
	uint32_t skbmark;
};

#define LSDN_KEY_SIZE (LSDN_MAX_MATCH_LEN * LSDN_MAX_MATCHES)
/* A single rule in lsdn_ruleset. Fill in the priority, match conditions and action. */
struct lsdn_rule {
	/* Match conditions, taken as logical conjunction of the match lsdn_match. */
	union lsdn_matchdata matches[LSDN_MAX_MATCHES];
	struct lsdn_action_desc action;
	uint32_t subprio;

	/** @privatesection */
	struct lsdn_ruleset *ruleset;
	struct lsdn_ruleset_prio *prio;
	struct lsdn_flower_rule *fl_rule;
	struct lsdn_list_entry sources_entry;
	UT_hash_handle hh;
};

/** Set of lsdn_rule organized by priorities and sub-priorities.
 * These rules will be transformed into a chain of TC flower filters.
 *
 * The rules may have the same priority only if they match against the same data. That is, they must have
 * the same match targets and masks. However, the rules within the same priority must be unique
 * (they must have different data), unless distinguished by subpriority
 *
 * Valid example:
 * ```
 *	1.0: dst_ip ~ 192.168.0.0/16
 *	1.0: dst_ip ~ 127.0.0.0/16
 * ```
 *
 * Invalid example (different match targets):
 * ```
 *	1.0: dst_ip ~ 192.168.0.0/16
 *	1.0: dst_ip ~ 192.168.0.0/32
 * ```
 *
 * Invalid example (duplicate rules):
 * ```
 *	1.0: dst_ip ~ 192.168.0.0/16
 *	1.0: dst_ip ~ 192.168.0.0/16
 * ```
 *
 * Valid example:
 * ```
 *	1.0: dst_ip ~ 192.168.0.0/16
 *	1.1: dst_ip ~ 192.168.0.0/16
 * ```
 *
 * Performance: For the flower filter chain to work efficiently, it is important to minimize the
 * number of flower filters and maximize the number of rules in one instance. You are guaranteed
 * that all rules sharing a priority will share a filter instance.
 */
struct lsdn_ruleset {
	/** Network interface where the rules apply. */
	struct lsdn_if *iface;
	/** LSDN context. */
	struct lsdn_context *ctx;
	uint32_t parent_handle;
	uint32_t chain;
	int prio_start;
	int prio_count;

	struct lsdn_ruleset_prio *hash_prios;
};

struct lsdn_ruleset_prio {
	uint16_t prio;
	enum lsdn_rule_target targets[LSDN_MAX_MATCHES];
	union lsdn_matchdata masks[LSDN_MAX_MATCHES];
	struct lsdn_ruleset *parent;
	struct lsdn_idalloc handle_alloc;

	UT_hash_handle hh;
	struct lsdn_flower_rule *hash_fl_rules;
};

/** Flower rule.
 * Represents a TC flower filter rule in kernel. */
struct lsdn_flower_rule {
	/** Match data. */
	union lsdn_matchdata matches[LSDN_MAX_MATCHES];
	/** Flower handle. */
	uint32_t fl_handle;
	/** List of rules that are combined into these flower rules */
	struct lsdn_list_entry sources_list;
	UT_hash_handle hh;
};

void lsdn_ruleset_init(
	struct lsdn_ruleset *ruleset, struct lsdn_context *ctx,
	struct lsdn_if *iface, uint32_t parent_handle, uint32_t chain, uint32_t prio_start, uint32_t prio_count);

struct lsdn_ruleset_prio* lsdn_ruleset_define_prio(struct lsdn_ruleset *rs, uint16_t prio);
struct lsdn_ruleset_prio* lsdn_ruleset_get_prio(struct lsdn_ruleset *rs, uint16_t main);
lsdn_err_t lsdn_ruleset_remove_prio(struct lsdn_ruleset_prio* prio);
/* Returns LSDNE_DUPLICATE if the rule is duplicate within a given priority.
 *
 * Even if an error is returned, some internal data will be set and your key will be masked.
 */
lsdn_err_t lsdn_ruleset_add(struct lsdn_ruleset_prio *prio, struct lsdn_rule *rule);
void lsdn_rule_apply_mask(
	struct lsdn_rule *r, enum lsdn_rule_target targets[], union lsdn_matchdata masks[]);
lsdn_err_t lsdn_ruleset_remove(struct lsdn_rule *rule);
void lsdn_ruleset_free(struct lsdn_ruleset *ruleset);

#define LSDN_MAX_ACT_PRIO 32

/* Represents an action mirroring packets to different entities. It is the analogy of
 * struct lsdn_flower for the broadcast case.
 *
 * Because the actions list in kernel is limited to TCA_ACT_MAX_PRIO (=32), we need to split
 * the actions among multiple filters for a long list. For this reasons, the user provides
 * callbacks for creating the actions. For composability, multiple lsdn_action_desc entries
 * can be given using lsdn_action_desc.
 */
struct lsdn_broadcast {
	struct lsdn_context *ctx;
	struct lsdn_if *iface;
	/* First priority usable for our filter chain */
	uint32_t chain;
	uint16_t free_prio;
	struct lsdn_list_entry filters_list;
};

struct lsdn_broadcast_action {
	struct lsdn_broadcast_filter *filter;
	size_t filter_entry_index;
	struct lsdn_action_desc action;
};

struct lsdn_broadcast_filter {
	struct lsdn_broadcast *broadcast;
	struct lsdn_list_entry filters_entry;
	int prio;
	size_t free_actions;
	/* The last action is reserved for the potential continue action,
	 * that is why we keep track of at most LSDN_MAX_PRIO - 1 actions.
	 */
	struct lsdn_broadcast_action *actions[LSDN_MAX_ACT_PRIO - 1];
};

void lsdn_broadcast_init(struct lsdn_broadcast *br, struct lsdn_context *ctx, struct lsdn_if *iface, int chain);
lsdn_err_t lsdn_broadcast_add(struct lsdn_broadcast *br, struct lsdn_broadcast_action *action, struct lsdn_action_desc desc);
lsdn_err_t lsdn_broadcast_remove(struct lsdn_broadcast_action *action);
lsdn_err_t lsdn_broadcast_free(struct lsdn_broadcast *br);

const char* lsdn_rule_target_name(enum lsdn_rule_target t);

#define LSDN_VR_SUBPRIO 0
struct lsdn_vr {
	struct lsdn_list_entry rules_entry;
	uint8_t pos;
	enum lsdn_state state;
	bool pending_free;
	enum lsdn_rule_target targets[LSDN_MAX_MATCHES];
	union lsdn_matchdata masks[LSDN_MAX_MATCHES];
	struct lsdn_rule rule;
	/* Used for duplicity checking */
	UT_hash_handle hh;
};

struct vr_prio {
	UT_hash_handle hh;
	uint16_t prio_num;
	size_t commited_count;
	struct lsdn_ruleset_prio *commited_prio;
	struct lsdn_list_entry rules_list;
};

struct lsdn_vr_action {
	struct lsdn_action_desc desc;
};

void lsdn_vr_do_free_all_rules(struct lsdn_virt *virt);
