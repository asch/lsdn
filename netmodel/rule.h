#ifndef _LSDN_RULE_H
#define _LSDN_RULE_H

#include "list.h"
#include "nettypes.h"

/*
 * relevant: include/net/flow_dissector.c
 * relevant: net/sched/cls_flower.c
 */

struct lsdn_port;
struct lsdn_rule;

enum lsdn_rule_target{
	LSDN_MATCH_ANYTHING,
	LSDN_MATCH_SRC_MAC,
	LSDN_MATCH_DST_MAC,
	LSDN_MATCH_ETHERTYPE
};

enum lsdn_action_id{
	/* Contiue processing the packet on a given LSDN logical port peer's*/
	LSDN_ACTION_PORT,
	/* Redirect the packet to a linux interface (s) egress*/
	LSDN_ACTION_IF,
	/* Drop the packet*/
	LSDN_ACTION_NONE,
	/* Continue classification with another ruleset */
	LSDN_ACTION_RULE
};

/* The actions have a bit different semantics than in tc. None of the actions
 * end the chain prematurely. So a DROP is more of a no-op and redirect is
 * redirect or mirror, depending on the circumstances */
struct lsdn_action{
	enum lsdn_action_id id;
	/* Packet may be mirrored into multiple actions */
	struct lsdn_action *next;
	union{
		/* Only valid for LSDN_ACTION_IF */
		const char *ifname;
		/* Only valid for LSDN_ACTION_PORT */
		struct lsdn_port *port;
		/* Only valid for LSDN_ACTION_RULE */
		struct lsdn_ruleset *rule;
	};
};

#define LSDN_MAX_MATCH_LEN 16
typedef union lsdn_matchdata_ {
	char bytes[LSDN_MAX_MATCH_LEN];
	lsdn_mac_t mac;
	uint16_t ethertype;
} lsdn_matchdata_t;

struct lsdn_rule{
	enum lsdn_rule_target target;
	struct lsdn_action action;
	lsdn_matchdata_t mask;
	lsdn_matchdata_t contents;

	int prio;
	struct lsdn_list_entry ruleset_list;
};

/*
 * The rules in a ruleset do not have to match against the same patterns.
 * It is job of the folding phase to produce more efficient data structure.
 */
struct lsdn_ruleset{	
	struct lsdn_list_entry rules;
	/* Linux interface that implements this ruleset. Managed by network.c. */
	struct lsdn_if *interface;
	int if_rules_created;
};

/* Set-up a rule matching zero contents agains full mask, with drop action
 * and 'anything' rule target. This function must be called before adding
 * the rule into ruleset
 */
void lsdn_rule_init(struct lsdn_rule* rule);
/* Set-up a drop action with no next action */
void lsdn_action_init(struct lsdn_action* action);
void lsdn_ruleset_init(struct lsdn_ruleset *ruleset);
void lsdn_ruleset_free(struct lsdn_ruleset *ruleset);
/* Lower numerical priority = rule matched first */
void lsdn_add_rule(struct lsdn_ruleset *ruleset, struct lsdn_rule* rule, int prio);
void lsdn_remove_rule(struct lsdn_rule* rule);

#endif
