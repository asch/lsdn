#include "private/rules.h"
#include "include/lsdn.h"

void lsdn_ruleset_init(struct lsdn_ruleset *ruleset, uint32_t chain, uint16_t prio)
{
	ruleset->chain = chain;
	ruleset->prio = prio;
	lsdn_list_init(&ruleset->rules_list);
}
/* Register a flower rule we have created */
struct lsdn_rule* lsdn_ruleset_add(struct lsdn_ruleset *ruleset,
		struct lsdn_list_entry *owner_list,
		uint32_t handle)
{
	// TODO: allocate and remember the rule
	return NULL;
}

void lsdn_broadcast_init(struct lsdn_broadcast *br, int chain)
{
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
static void lsdn_flush_action_list(
	struct lsdn_context *ctx,
	struct lsdn_broadcast *br, struct lsdn_broadcast_filter* br_filter,
	struct lsdn_if *iface)
{
	struct lsdn_filter *filter = lsdn_filter_flower_init(
		iface->ifindex, MAIN_RULE_HANDLE, LSDN_INGRESS_HANDLE, br->chain, br_filter->prio);
	size_t order = 1;
	int err;

	lsdn_flower_actions_start(filter);
	for(size_t i = 0; i < LSDN_MAX_PRIO - 1; i++) {
		struct lsdn_broadcast_action *action = &br_filter->actions[i];
		if (!action->used)
			continue;
		//printf("Adding action to filter at %d if %s\n", order, iface->ifname);
		action->create(filter, order, action->user1, action->user2);
		order += action->actions_count;
	}
	lsdn_action_continue(filter, order);
	lsdn_filter_actions_end(filter);
	err = lsdn_filter_create(ctx->nlsock, filter);
	if (err)
		abort();
	lsdn_filter_free(filter);
}

void lsdn_broadcast_add(
	struct lsdn_context *ctx,
	struct lsdn_broadcast *br, struct lsdn_list_entry *owner_list, struct lsdn_if *iface,
	lsdn_mkaction_fn create,  size_t actions, void *user1, void *user2)
{
	struct lsdn_broadcast_filter *f;
	struct lsdn_broadcast_action *a;
	if (!lsdn_find_free_action(br, actions, &f, &a))
		abort();
	f->free_actions -= actions;
	a->used =  true;
	a->create = create;
	a->user1 = user1;
	a->user2 = user2;
	a->actions_count = actions;
	lsdn_list_init_add(owner_list, &a->owned_actions_entry);
	lsdn_flush_action_list(ctx, br, f, iface);
}
