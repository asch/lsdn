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

			if (a == last_action)
				lsdn_action_mirred_add(filter, 1, TC_ACT_STOLEN, TCA_EGRESS_REDIR, if_index);
			else
				lsdn_action_mirred_add(filter, 1, TC_ACT_PIPE, TCA_EGRESS_MIRROR, if_index);
		}
	}
	if(!last_action) {
		lsdn_action_drop(filter, 1, TC_ACT_SHOT);
	}
}

//void runcmd(const char *format, ...)
//{
//	char cmdbuf[1024];
//	va_list args;
//	va_start(args, format);
//	vsnprintf(cmdbuf, sizeof(cmdbuf), format, args);
//	va_end(args);
//	printf("Running: %s\n", cmdbuf);
//	system(cmdbuf);
//}
