#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>
#include "private/tc.h"
#include "private/port.h"
#include "include/errors.h"

static int is_action_valid(struct lsdn_action *a)
{
	return a->id != LSDN_ACTION_NONE &&
		!(a->id == LSDN_ACTION_PORT && a->port->peer == NULL);
}

lsdn_err_t actions_for(struct lsdn_action *action, struct lsdn_filter *filter)
{
	/* The last action sending the packet somewhere */
	struct lsdn_action *last_action = NULL;
	for(struct lsdn_action *a = action; a; a = a->next) {
		if(is_action_valid(a))
			last_action = a;
	}

	int action_id = 1;
	/* TODO: Limit actions to max TCA_ACT_MAX_PRIO*/
	for(struct lsdn_action *a = action; a; a = a->next) {
		if(a->id == LSDN_ACTION_NONE)
			continue;

		int ifindex = 0;
		const char* ifname = NULL;

		switch(a->id){
		case LSDN_ACTION_PORT:
			if(a->port->peer) {
				ifindex= a->port->peer->ruleset->interface.ifindex;
				ifname = a->port->peer->ruleset->interface.ifname;
			}
			break;
		case LSDN_ACTION_IF:
			ifname = a->ifname;
			ifindex = if_nametoindex(ifname);
			if(ifindex == 0)
				return LSDNE_NOIF;
			break;
		case LSDN_ACTION_RULE:
			ifname = a->rule->interface.ifname;
			ifindex = a->rule->interface.ifindex;
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
	return LSDNE_OK;
}

