#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tc.h"
#include "internal.h"

static int is_action_valid(struct lsdn_action *a)
{
	return a->id != LSDN_ACTION_NONE &&
		!(a->id == LSDN_ACTION_PORT && a->port->peer == NULL);
}

void actions_for(struct lsdn_action *action, struct lsdn_filter *filter)
{
	/* The last action sending the packet somewhere */
	struct lsdn_action *last_action = NULL;
	for(struct lsdn_action *a = action; a; a = a->next) {
		if(is_action_valid(a))
			last_action = a;
	}

	int action_id = 1;
	/* TODO: Limit under TCA_ACT_MAX_PRIO */
	for(struct lsdn_action *a = action; a; a = a->next) {
		if(a->id == LSDN_ACTION_NONE)
			continue;

		const char *ifname = NULL;
		switch(a->id){
		case LSDN_ACTION_PORT:
			if(a->port->peer)
				ifname = a->port->peer->ruleset->interface->ifname;
			break;
		case LSDN_ACTION_IF:
			ifname = a->ifname;
			break;
		case LSDN_ACTION_RULE:
			ifname = a->rule->interface->ifname;
			break;
		default:
			/* make static analysis happy */
			break;
		}
		if(ifname) {
			uint32_t if_index = if_nametoindex(ifname);

			if (a == last_action){
				printf("Redirecting to %s (%d)\n", ifname, if_index);
				lsdn_action_mirred_add(filter, action_id++, TC_ACT_STOLEN, TCA_EGRESS_REDIR, if_index);
			} else {
				printf("Mirrorring to %s (%d)\n", ifname, if_index);
				lsdn_action_mirred_add(filter, action_id++, TC_ACT_PIPE, TCA_EGRESS_MIRROR, if_index);
			}
		}
	}
	if(!last_action) {
		printf("Dropping\n");
		lsdn_action_drop(filter, action_id++, TC_ACT_SHOT);
	}
}

