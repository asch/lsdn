#include "private/rules.h"
#include "include/lsdn.h"
#include "private/log.h"

void lsdn_ruleset_init(
	struct lsdn_ruleset *ruleset, struct lsdn_context *ctx,
	struct lsdn_if *iface, uint32_t chain, uint16_t prio)
{
	ruleset->iface = iface;
	ruleset->chain = chain;
	ruleset->prio = prio;
	ruleset->ctx = ctx;
	lsdn_idalloc_init(&ruleset->handles, 1, 0xFFFF);
	lsdn_list_init(&ruleset->rules_list);
}

void lsdn_ruleset_free(struct lsdn_ruleset *ruleset)
{
	assert(lsdn_is_list_empty(&ruleset->rules_list));
}

void lsdn_ruleset_remove(struct lsdn_rule *rule)
{
	if(!rule->ruleset->ctx->disable_decommit) {
		lsdn_log(LSDNL_RULES, "ruleset_remove(iface=%s,chain=%d, prio=%d, handle=0x%x)\n",
			 rule->ruleset->iface->ifname, rule->ruleset->chain, rule->ruleset->prio, rule->handle);
		int err = lsdn_filter_delete(
			rule->ruleset->ctx->nlsock, rule->ruleset->iface->ifindex, rule->handle,
			LSDN_INGRESS_HANDLE, rule->ruleset->chain, rule->ruleset->prio);
		if (err)
			abort();
	}
	lsdn_list_remove(&rule->rules_entry);
}

struct lsdn_filter* lsdn_ruleset_add(struct lsdn_ruleset *ruleset, struct lsdn_rule *rule)
{
	uint32_t handle;
	if (!lsdn_idalloc_get(&ruleset->handles, &handle))
		return NULL;

	struct lsdn_filter *filter = lsdn_filter_flower_init(
		ruleset->iface->ifindex, handle, LSDN_INGRESS_HANDLE,
		ruleset->chain, ruleset->prio);
	if (!filter) {
		lsdn_idalloc_return(&ruleset->handles, handle);
		return NULL;
	}
	rule->handle = handle;
	rule->filter = filter;
	rule->ruleset = ruleset;

	return filter;
}

void lsdn_ruleset_add_finish(struct lsdn_rule *rule)
{
	lsdn_log(LSDNL_RULES, "ruleset_add(iface=%s,chain=%d, prio=%d, handle=0x%x)\n",
		 rule->ruleset->iface->ifname, rule->ruleset->chain, rule->ruleset->prio, rule->handle);
	int err = lsdn_filter_create(rule->ruleset->ctx->nlsock, rule->filter);
	if (err)
		abort();
	lsdn_filter_free(rule->filter);
	lsdn_list_init_add(&rule->ruleset->rules_list, &rule->rules_entry);
	rule->filter = NULL;
}

void lsdn_broadcast_init(struct lsdn_broadcast *br, struct lsdn_context *ctx, struct lsdn_if *iface, int chain)
{
	br->ctx = ctx;
	br->iface = iface;
	br->chain = chain;
	br->free_prio = 1;
	lsdn_list_init(&br->filters_list);
}

static bool lsdn_find_free_action(
	struct lsdn_broadcast *br, size_t actions,
	struct lsdn_broadcast_filter** out_filter, size_t* out_action_index)
{
	lsdn_foreach(br->filters_list, filters_entry, struct lsdn_broadcast_filter, f) {
		if (f->free_actions >= actions) {
			for(size_t i = 0; i<LSDN_MAX_PRIO - 1; i++) {
				struct lsdn_broadcast_action **a = &f->actions[i];
				if(!*a) {
					*out_filter = f;
					*out_action_index = i;
					return true;
				}
			}
			/* should not happen if there are free actions */
			abort();
		}
	}

	// create new filter

	struct lsdn_broadcast_filter *f = malloc(sizeof(*f));
	if(!f)
		return false;

	lsdn_list_init_add(&br->filters_list, &f->filters_entry);
	f->broadcast = br;
	f->free_actions = LSDN_MAX_PRIO - 1;
	f->prio = br->free_prio++;
	for(size_t i = 0; i<LSDN_MAX_PRIO - 1; i++) {
		f->actions[i] = NULL;
	}
	*out_filter = f;
	*out_action_index = 0;

	return true;
}

#define MAIN_RULE_HANDLE 1

/** Update the broadcast filter and all it's actions. */
static void lsdn_flush_action_list(struct lsdn_broadcast_filter* br_filter)
{
	struct lsdn_broadcast *br = br_filter->broadcast;
	struct lsdn_filter *filter = lsdn_filter_flower_init(
		br->iface->ifindex,
		MAIN_RULE_HANDLE, LSDN_INGRESS_HANDLE, br->chain, br_filter->prio);
	lsdn_filter_set_update(filter);
	size_t order = 1;
	int err;

	lsdn_flower_actions_start(filter);
	for(size_t i = 0; i < LSDN_MAX_PRIO - 1; i++) {
		struct lsdn_broadcast_action *action = br_filter->actions[i];
		if (!action)
			continue;
		//printf("Adding action to filter at %d if %s\n", order, iface->ifname);
		action->action.fn(filter, order, action->action.user);
		order += action->action.actions_count;
	}
	lsdn_action_continue(filter, order);
	lsdn_filter_actions_end(filter);
	err = lsdn_filter_create(br->ctx->nlsock, filter);
	if (err)
		abort();
	lsdn_filter_free(filter);
}

void lsdn_broadcast_add(struct lsdn_broadcast *br, struct lsdn_broadcast_action *action, struct lsdn_action_desc desc)
{
	struct lsdn_broadcast_filter *f;
	size_t i;
	if (!lsdn_find_free_action(br, desc.actions_count, &f, &i))
		abort();
	f->free_actions -= desc.actions_count;
	f->actions[i] = action;
	action->action = desc;
	action->filter = f;
	action->filter_entry_index = i;
	lsdn_flush_action_list(f);
}

void lsdn_broadcast_remove(struct lsdn_broadcast_action *action)
{
	action->filter->free_actions += action->action.actions_count;
	action->filter->actions[action->filter_entry_index] = NULL;
	if(!action->filter->broadcast->ctx->disable_decommit)
		lsdn_flush_action_list(action->filter);
}

void lsdn_broadcast_free(struct lsdn_broadcast *br)
{
	lsdn_foreach(br->filters_list, filters_entry, struct lsdn_broadcast_filter, f)
	{
		if(!br->ctx->disable_decommit) {
			int err = lsdn_filter_delete(
				br->ctx->nlsock, br->iface->ifindex,
				MAIN_RULE_HANDLE, LSDN_INGRESS_HANDLE, br->chain, f->prio);
			if (err)
				abort();
		}
		free(f);
	}
}
