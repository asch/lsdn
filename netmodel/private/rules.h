#ifndef LSDN_RULES_H
#define LSDN_RULES_H

#include "list.h"
#include <stdbool.h>
#include "nl.h"
#include "idalloc.h"

/* Represents a flower filter instance where the rules can be added and removed. */
struct lsdn_ruleset {
	struct lsdn_if *iface;
	struct lsdn_context *ctx;
	uint32_t chain;
	uint16_t prio;
	struct lsdn_list_entry rules_list;
	struct lsdn_idalloc handles;
};

struct lsdn_rule {
	struct lsdn_list_entry rules_entry;
	uint32_t handle;
	struct lsdn_ruleset *ruleset;
	struct lsdn_filter *filter;
};

void lsdn_ruleset_init(
	struct lsdn_ruleset *ruleset, struct lsdn_context *ctx,
	struct lsdn_if *iface, uint32_t chain, uint16_t prio);
struct lsdn_filter* lsdn_ruleset_add(struct lsdn_ruleset *ruleset, struct lsdn_rule *rule);
void lsdn_ruleset_add_finish(struct lsdn_rule *rule);
void lsdn_ruleset_remove(struct lsdn_rule *rule);
void lsdn_ruleset_free(struct lsdn_ruleset *ruleset);


#define LSDN_MAX_PRIO 32

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

typedef void (*lsdn_mkaction_fn)(struct lsdn_filter *filter, uint16_t order, void *user);
/* Describes a sequence of TC actions */
struct lsdn_action_desc {
	size_t actions_count;
	/* Callback to create the actions on the filter rule */
	lsdn_mkaction_fn fn;
	void *user;
};

struct lsdn_broadcast_action {
	struct lsdn_list_entry owned_actions_entry;
	struct lsdn_broadcast_filter *filter;
	bool used;
	struct lsdn_action_desc action;
};

struct lsdn_broadcast_filter {
	struct lsdn_list_entry filters_entry;
	int prio;
	size_t free_actions;
	struct lsdn_broadcast_action actions[LSDN_MAX_PRIO - 1];
};

void lsdn_broadcast_init(struct lsdn_broadcast *br, struct lsdn_context *ctx, struct lsdn_if *iface, int chain);
void lsdn_broadcast_add(struct lsdn_broadcast *br, struct lsdn_broadcast_action *action, struct lsdn_action_desc desc);
void lsdn_broadcast_remove(struct lsdn_broadcast_action *action);
void lsdn_broadcast_free(struct lsdn_broadcast *br);


#endif
