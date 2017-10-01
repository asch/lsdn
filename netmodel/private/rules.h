#ifndef LSDN_RULES_H
#define LSDN_RULES_H

#include "list.h"
#include <stdbool.h>
#include "nl.h"

/* Represents a flower filter instance where the rules are created on behalf of different actors.
 * An example of such a flower instance is the VXLAN interface, where each rule matches a given
 * VNI and is owned by the physical attachment. Other example is the dummy interface responsible
 * for switching of packets (as featured in the static VXLAN), each rule in this instance is owned
 * by a virt participating in the network. */
struct lsdn_ruleset {
	uint32_t chain;
	uint16_t prio;
	struct lsdn_list_entry rules_list;
};

struct lsdn_rule {
	struct lsdn_list_entry rules_entry;
	struct lsdn_list_entry owned_rules_entry;
	uint32_t handle;
};

void lsdn_ruleset_init(struct lsdn_ruleset *ruleset, uint32_t chain, uint16_t prio);
/* Register a flower rule we have created */
struct lsdn_rule* lsdn_ruleset_add(
	struct lsdn_ruleset *ruleset,
	struct lsdn_list_entry *owner_list,
	uint32_t handle);


#define LSDN_MAX_PRIO 32

/* Represents an action mirroring packets to different entities. It is the analogy of
 * struct lsdn_flower for the broadcast case.
 *
 * Because the actions list in kernel is limited to TCA_ACT_MAX_PRIO (=32), we need to split
 * the actions among multiple filters for a long list.
 */
struct lsdn_broadcast {
	/* First priority usable for our filter chain */
	uint32_t chain;
	uint16_t free_prio;
	struct lsdn_list_entry filters_list;
};

typedef void (*lsdn_mkaction_fn)(
	struct lsdn_filter *filter,
	uint16_t order, void *user1, void *user2);

struct lsdn_broadcast_action {
	struct lsdn_list_entry owned_actions_entry;
	struct lsdn_broadcast_filter *filter;
	bool used;
	size_t actions_count;
	void *owner;
	lsdn_mkaction_fn create;
	void *user1;
	void *user2;
};

struct lsdn_broadcast_filter {
	struct lsdn_list_entry filters_entry;
	int prio;
	size_t free_actions;
	struct lsdn_broadcast_action actions[LSDN_MAX_PRIO - 1];
};

void lsdn_broadcast_init(struct lsdn_broadcast *br, int chain);
void lsdn_broadcast_add(struct lsdn_context *ctx,
	struct lsdn_broadcast *br, struct lsdn_list_entry *owner_list, struct lsdn_if *iface,
	lsdn_mkaction_fn create, size_t actions, void *user1, void *user2);



#endif
