#include "private/rules.h"
#include "include/lsdn.h"

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
	// TODO: remove the filter and remove ourselves from the list
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
	rule->filter = filter;
	rule->ruleset = ruleset;

	return filter;
}

void lsdn_ruleset_add_finish(struct lsdn_rule *rule)
{
	int err = lsdn_filter_create(rule->ruleset->ctx->nlsock, rule->filter);
	if (err)
		abort();
	lsdn_filter_free(rule->filter);
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
	struct lsdn_broadcast_filter** out_filter, struct lsdn_broadcast_action** out_action)
{
	lsdn_foreach(br->filters_list, filters_entry, struct lsdn_broadcast_filter, f) {
		if (f->free_actions >= actions) {
			for(size_t i = 0; i<LSDN_MAX_PRIO - 1; i++) {
				struct lsdn_broadcast_action *a = &f->actions[i];
				if(!a->used) {
					*out_filter = f;
					*out_action = a;
					return true;
				}
			}
			/* should not happen if there are free actions */
			abort();
		}
	}

	struct lsdn_broadcast_filter *f = malloc(sizeof(*f));
	if(!f)
		return false;

	// create new filter
	lsdn_list_init_add(&br->filters_list, &f->filters_entry);
	f->free_actions = LSDN_MAX_PRIO - 1;
	f->prio = br->free_prio++;
	for(size_t i = 0; i<LSDN_MAX_PRIO - 1; i++) {
		f->actions[i].used = false;
		f->actions[i].filter = f;
	}
	*out_filter = f;
	*out_action = &f->actions[0];

	return true;
}

#define MAIN_RULE_HANDLE 1

/** Update the broadcast filter and all it's actions. */
static void lsdn_flush_action_list(struct lsdn_broadcast *br, struct lsdn_broadcast_filter* br_filter)
{
	struct lsdn_filter *filter = lsdn_filter_flower_init(
		br->iface->ifindex,
		MAIN_RULE_HANDLE, LSDN_INGRESS_HANDLE, br->chain, br_filter->prio);
	size_t order = 1;
	int err;

	lsdn_flower_actions_start(filter);
	for(size_t i = 0; i < LSDN_MAX_PRIO - 1; i++) {
		struct lsdn_broadcast_action *action = &br_filter->actions[i];
		if (!action->used)
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
	struct lsdn_broadcast_action *a;
	if (!lsdn_find_free_action(br, desc.actions_count, &f, &a))
		abort();
	f->free_actions -= desc.actions_count;
	a->used =  true;
	a->action = desc;
	lsdn_flush_action_list(br, f);
}

void lsdn_broadcast_remove(struct lsdn_broadcast_action *action)
{
	// TODO
}

void lsdn_broadcast_free(struct lsdn_broadcast *br)
{
	// TODO
}
