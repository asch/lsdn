#include "private/rules.h"
#include "include/lsdn.h"
#include "private/log.h"
#include "private/lsdn.h"
#include <string.h>

/* TODO: convert Uthash OOM to something "safe" */

void lsdn_action_init(struct lsdn_action_desc *action, size_t count, lsdn_mkaction_fn fn, void *user)
{
	action->actions_count = count;
	action->fn =fn;
	action->user = user;
}

void lsdn_ruleset_init(
	struct lsdn_ruleset *ruleset, struct lsdn_context *ctx,
	struct lsdn_if *iface, uint32_t chain, uint32_t prio_start, uint32_t prio_count)
{
	ruleset->iface = iface;
	ruleset->chain = chain;
	ruleset->prio_start = prio_start;
	ruleset->prio_count = prio_count;
	ruleset->ctx = ctx;
	ruleset->hash_prios = NULL;
}

static char hexdigit(uint8_t val)
{
	return (val < 10) ? '0' + val : 'A' + (val - 10);
}

static void hexdump(char *buf, uint8_t *mem, size_t count)
{
	while (count) {
		*(buf++) = hexdigit((*mem >> 4) & 0xF);
		*(buf++) = hexdigit(*mem & 0xF);
		count--; mem++;
	}
	*(buf++) = 0;
}

static void dump_rule(struct lsdn_rule *rule)
{
	if (!lsdn_log_enabled(LSDNL_RULES))
		return;
	for(int i = 0; i<LSDN_MAX_MATCHES; i++) {
		char value[LSDN_MAX_MATCH_LEN*2 + 1];
		char mask[LSDN_MAX_MATCH_LEN*2 + 1];
		hexdump(value, (uint8_t*) rule->matches[i].bytes, LSDN_MAX_MATCH_LEN);
		hexdump(mask, (uint8_t*) rule->prio->masks[i].bytes, LSDN_MAX_MATCH_LEN);
		lsdn_log(LSDNL_RULES, " %3d: %s & %s\n", rule->prio->targets[i], value, mask);
	}
}

bool lsdn_target_supports_masking(enum lsdn_rule_target target)
{
	switch(target) {
	case LSDN_MATCH_NONE:
	case LSDN_MATCH_ENC_KEY_ID:
		return false;
	default:
		return true;
	}
}

struct lsdn_ruleset_prio* lsdn_ruleset_define_prio(struct lsdn_ruleset *rs, uint16_t main)
{
	struct lsdn_ruleset_prio *p;
	HASH_FIND(hh, rs->hash_prios, &main, sizeof(main), p);
	if (p)
		return NULL;

	p = malloc(sizeof(*p));
	if (!p)
		return NULL;
	p->hash_fl_rules = NULL;
	p->prio = main;
	p->parent = rs;
	bzero(&p->targets, sizeof(p->targets));
	bzero(&p->masks, sizeof(p->masks));
	lsdn_idalloc_init(&p->handle_alloc, 1, 0xFFFF);
	/* warning: HASH_ADD_INT is really only for ints, not uint16_t */
	HASH_ADD(hh, rs->hash_prios, prio, sizeof(p->prio), p);
	return p;
}

struct lsdn_ruleset_prio *lsdn_ruleset_get_prio(struct lsdn_ruleset *rs, uint16_t main)
{
	struct lsdn_ruleset_prio *p;
	HASH_FIND(hh, rs->hash_prios, &main, sizeof(main), p);
	return p;
}

void lsdn_ruleset_free(struct lsdn_ruleset *ruleset)
{
	struct lsdn_ruleset_prio *prio, *prio_tmp;
	HASH_ITER(hh, ruleset->hash_prios, prio, prio_tmp) {
		assert(HASH_COUNT(prio->hash_fl_rules) == 0);
		HASH_DEL(ruleset->hash_prios, prio);
		free(prio);
	}
}

/* Update the flower rule in TC */
static lsdn_err_t flush_fl_rule(struct lsdn_flower_rule *fl, struct lsdn_ruleset_prio *prio, bool update)
{
	struct lsdn_ruleset *ruleset = prio->parent;
	struct lsdn_filter *filter = lsdn_filter_flower_init(
		ruleset->iface->ifindex, fl->fl_handle, LSDN_INGRESS_HANDLE,
		ruleset->chain, prio->prio + ruleset->prio_start);
	if (update)
		lsdn_filter_set_update(filter);

	lsdn_log(LSDNL_RULES, "fl_%s(handle=0x%x)\n", update ? "update" : "create", fl->fl_handle);

	if (!filter) {
		return LSDNE_NETLINK;
	}

	for(int i = 0; i<LSDN_MAX_MATCHES; i++) {
		switch(prio->targets[i]) {
		case LSDN_MATCH_DST_MAC:
			lsdn_flower_set_dst_mac(filter, fl->matches[i].mac.chr, prio->masks[i].mac.chr);
			break;
		case LSDN_MATCH_SRC_MAC:
			lsdn_flower_set_src_mac(filter, fl->matches[i].mac.chr, prio->masks[i].mac.chr);
			break;
		case LSDN_MATCH_ENC_KEY_ID:
			lsdn_flower_set_enc_key_id(filter, fl->matches[i].enc_key_id);
			break;
		case LSDN_MATCH_NONE:
			break;
		default:
			// TODO: obvious
			abort();
		}
	}

	lsdn_flower_actions_start(filter);
	size_t order = 1;
	lsdn_foreach(fl->sources_list, sources_entry, struct lsdn_rule, r) {
		assert (order + r->action.actions_count <= LSDN_MAX_PRIO);
		r->action.fn(filter, order, r->action.user);
		order += r->action.actions_count;
	}

	lsdn_flower_actions_end(filter);
	lsdn_err_t err = lsdn_filter_create(ruleset->ctx->nlsock, filter);
	if (err != LSDNE_OK) {
		lsdn_filter_free(filter);
		return err;
	}

	lsdn_filter_free(filter);

	return LSDNE_OK;
}

static void free_fl_rule(struct lsdn_flower_rule *fl, struct lsdn_ruleset_prio *prio)
{
	struct lsdn_ruleset *rs = prio->parent;
	lsdn_log(LSDNL_RULES, "fl_delete(handle=0x%x)\n", fl->fl_handle);
	if (!prio->parent->ctx->disable_decommit) {
		lsdn_err_t err = lsdn_filter_delete(
			rs->ctx->nlsock, rs->iface->ifindex, fl->fl_handle,
			LSDN_INGRESS_HANDLE, rs->chain, prio->prio + rs->prio_start);
		if (err != LSDNE_OK)
			abort();
	}

	HASH_DEL(prio->hash_fl_rules, fl);
	free(fl);
}

void lsdn_ruleset_remove(struct lsdn_rule *rule)
{
	lsdn_log(LSDNL_RULES, "ruleset_remove(iface=%s, chain=%d, prio=0x%x, handle=0x%x)\n",
		rule->ruleset->iface->ifname, rule->ruleset->chain, rule->prio->prio,
		rule->fl_rule->fl_handle);
	lsdn_list_remove(&rule->sources_entry);
	if (lsdn_is_list_empty(&rule->fl_rule->sources_list)) {
		free_fl_rule(rule->fl_rule, rule->prio);
	} else {
		flush_fl_rule(rule->fl_rule, rule->prio, true);
	}
	rule->fl_rule = NULL;
}

static void hard_mask(char *value, size_t valsize)
{
	assert(valsize < LSDN_MAX_MATCH_LEN);
	bzero(value + valsize, LSDN_MAX_MATCH_LEN - valsize);
}

static void mask_key(struct lsdn_rule *r)
{
	struct lsdn_ruleset_prio *p = r->prio;
	for(int i = 0; i<LSDN_MAX_MATCHES; i++) {
		if (!lsdn_target_supports_masking(p->targets[i])) {
			switch (p->targets[i]) {
			case LSDN_MATCH_NONE:
				hard_mask(r->matches[i].bytes, 0);
			break;
			case LSDN_MATCH_ENC_KEY_ID:
				hard_mask(r->matches[i].bytes, sizeof(r->matches[i].enc_key_id));
			break;
			default:
				abort();
			}
		} else {
			for(int j = 0; j<LSDN_MAX_MATCH_LEN; j++) {
				r->matches[i].bytes[j] &= p->masks[i].bytes[j];
			}
		}
	}
}

lsdn_err_t lsdn_ruleset_add(struct lsdn_ruleset_prio *prio, struct lsdn_rule *rule)
{
	bool update = true;
	lsdn_err_t err = LSDNE_OK;
	rule->prio = prio;
	rule->ruleset = prio->parent;
	mask_key(rule);
	lsdn_log(LSDNL_RULES, "ruleset_add(iface=%s, chain=%d, prio=0x%x)\n",
		rule->ruleset->iface->ifname, rule->ruleset->chain, prio->prio);
	dump_rule(rule);

	struct lsdn_flower_rule *fl;
	HASH_FIND(hh, rule->prio->hash_fl_rules, rule->matches, sizeof(rule->matches), fl);
	if (!fl) {
		uint32_t handle;
		if (!lsdn_idalloc_get(&rule->prio->handle_alloc, &handle))
			return LSDNE_NOMEM;

		fl = malloc(sizeof(*fl));
		if (!fl) {
			lsdn_idalloc_return(&rule->prio->handle_alloc, handle);
			return LSDNE_NOMEM;
		}
		memcpy(fl->matches, rule->matches, sizeof(fl->matches));
		lsdn_list_init(&fl->sources_list);
		fl->fl_handle = handle;
		HASH_ADD(hh, rule->prio->hash_fl_rules, matches, sizeof(fl->matches), fl);
		update = false;
	}
	rule->fl_rule = fl;

	/* insert sorted into the sources list */
	struct lsdn_list_entry *nearest_lower = &fl->sources_list;
	lsdn_foreach(fl->sources_list, sources_entry, struct lsdn_rule, r) {
		if (r->subprio == rule->subprio) {
			/* there is no need to free `fl`, we won't be here if it is newly allocated */
			return LSDNE_DUPLICATE;
		}
		if (r->subprio >= rule->subprio)
			break;
		nearest_lower = &r->sources_entry;
	}
	lsdn_list_init_add(nearest_lower, &rule->sources_entry);

	err = flush_fl_rule(fl, prio, update);
	if (err != LSDNE_OK) {
		lsdn_list_remove(&rule->sources_entry);
		free_fl_rule(fl, prio);
		return err;
	}

	return err;
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
	lsdn_flower_actions_end(filter);
	err = lsdn_filter_create(br->ctx->nlsock, filter);
	if (err != LSDNE_OK)
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
			lsdn_err_t err = lsdn_filter_delete(
				br->ctx->nlsock, br->iface->ifindex,
				MAIN_RULE_HANDLE, LSDN_INGRESS_HANDLE, br->chain, f->prio);
			if (err != LSDNE_OK)
				abort();
		}
		free(f);
	}
}
