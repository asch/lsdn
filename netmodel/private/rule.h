#ifndef _LSDN_RULE_H_PRIVATE_
#define _LSDN_RULE_H_PRIVATE_

#include "../include/nettypes.h"
#include "../private/nl.h"
#include "list.h"

/*
 * relevant: include/net/flow_dissector.c
 * relevant: net/sched/cls_flower.c
 */

struct lsdn_virt;
struct lsdn_rule;

enum lsdn_match_target {
	LSDN_MATCH_SRC_MAC,
	LSDN_MATCH_DST_MAC,
	LSDN_MATCH_MAX
};

#define LSDN_MAX_MATCH_LEN 16
typedef union lsdn_matchdata_ {
	char bytes[LSDN_MAX_MATCH_LEN];
	lsdn_mac_t mac;
} lsdn_matchdata_t;

struct lsdn_match{
	/* number of significant bits from the left (similar to IP mask) */
	uint8_t mask_len;
	lsdn_matchdata_t data;
};

struct lsdn_ruleset {
	/* list of fields agains which this ruleset matches */
	enum lsdn_match_target target;
	struct lsdn_list_entry rules_list;

	struct lsdn_if interface;
};

enum lsdn_action_id {
	/* Redirect the packet to a linux interface (s) egress */
	LSDN_ACTION_IF,
	/* Drop the packet */
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
		/* Only valid for LSDN_ACTION_RULE */
		struct lsdn_ruleset *rule;
	};
};

struct lsdn_rule{
	struct lsdn_match match;
	struct lsdn_action action;

	struct lsdn_list_entry rule_instance_entry;
	struct lsdn_virt_rule *rule_template;

	struct lsdn_list_entry rules_entry;
};

/* Set-up a rule matching anything (mask 0). This function must be called before adding
 * the rule into ruleset
 */
void lsdn_rule_init(struct lsdn_rule *rule);
void lsdn_rule_free(struct lsdn_rule *rule);

/* Set-up a drop action with no next action */
void lsdn_action_init(struct lsdn_action *action);
void lsdn_ruleset_init(struct lsdn_ruleset *ruleset);
void lsdn_ruleset_free(struct lsdn_ruleset *ruleset);
/* Lower numerical priority = rule matched first */
void lsdn_add_rule(struct lsdn_ruleset *ruleset, struct lsdn_rule *rule, int prio);
void lsdn_remove_rule(struct lsdn_rule *rule);

#endif
