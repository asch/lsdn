#include "private/rule.h"
#include <memory.h>
#include "include/util.h"

void lsdn_rule_init(struct lsdn_rule *rule)
{
	lsdn_list_init(&rule->rules_entry);
	lsdn_list_init(&rule->rule_instance_entry);

	rule->rule_template = NULL;
	rule->match.mask_len = 0;
	bzero(rule->match.data.bytes, LSDN_MAX_MATCH_LEN);
}
void lsdn_action_init(struct lsdn_action *action)
{
	action->id = LSDN_ACTION_NONE;
	action->next = NULL;
}

void lsdn_ruleset_init(struct lsdn_ruleset *ruleset)
{
	ruleset->target = LSDN_MATCH_DST_MAC;
	lsdn_list_init(&ruleset->rules_list);
	lsdn_init_if(&ruleset->interface);
}

void lsdn_ruleset_free(struct lsdn_ruleset *ruleset)
{
	assert(lsdn_is_list_empty(&ruleset->rules_list));
}
void lsdn_rule_free(struct lsdn_rule *rule)
{
	if(!lsdn_is_list_empty(&rule->rules_entry))
		lsdn_list_remove(&rule->rules_entry);
}

void lsdn_add_rule(struct lsdn_ruleset *ruleset, struct lsdn_rule *rule, int prio){
	/* TODO: check for collisions */
	lsdn_list_add(&ruleset->rules_list, &rule->rules_entry);
}

void lsdn_remove_rule(struct lsdn_rule *rule)
{
	lsdn_list_remove(&rule->rules_entry);
}
