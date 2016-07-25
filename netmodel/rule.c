#include "rule.h"
#include "memory.h"

void lsdn_rule_init(struct lsdn_rule *rule)
{
	lsdn_list_init(&rule->ruleset_list);
	rule->target = LSDN_MATCH_ANYTHING;
	lsdn_action_init(&rule->action);
	memset(&rule->mask, 0xFF, LSDN_MAX_MATCH_LEN);
	memset(&rule->contents, 0, LSDN_MAX_MATCH_LEN);
}
void lsdn_action_init(struct lsdn_action *action){
	action->id = LSDN_ACTION_NONE;
	action->next = NULL;
}

void lsdn_ruleset_init(struct lsdn_ruleset *ruleset){
	ruleset->if_rules_created = 0;
	ruleset->interface = NULL;
	lsdn_list_init(&ruleset->rules);
	lsdn_list_init(&ruleset->node_rules);
}

void lsdn_ruleset_free(struct lsdn_ruleset *ruleset){
	assert(lsdn_is_list_empty(&ruleset->rules));
}
void lsdn_add_rule(struct lsdn_ruleset *ruleset, struct lsdn_rule *rule, int prio){
	/* find the nearest lowest priority */
	struct lsdn_list_entry *nearest = ruleset->rules.next;
	lsdn_foreach_list(ruleset->rules, ruleset_list, struct lsdn_rule, c) {
		if(c->prio < prio)
			nearest = &c->ruleset_list;
	}
	rule->prio = prio;
	lsdn_list_add(nearest, &rule->ruleset_list);
}

void lsdn_remove_rule(struct lsdn_rule *rule)
{
	lsdn_list_remove(&rule->ruleset_list);
}
