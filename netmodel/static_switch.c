#include <stdlib.h>
#include <arpa/inet.h>
#include "include/static_switch.h"
#include "private/rule.h"
#include "private/tc.h"
#include "private/port.h"
#include "private/node.h"
#include "private/list.h"

enum {RULE_BROADCAST, RULE_OTHER};

static struct lsdn_port_ops port_ops;

struct switch_port{
	struct lsdn_port port;
	/* Only used if broadcast packet handling is enabled,
	 * otherwise the port's ruleset is directly the switch_ruleset */
	struct lsdn_ruleset broadcast_ruleset;
	struct lsdn_rule broadcast_rules[2];
	/* List of port-1 actions mirroring to all port but the incoming one. */
	struct lsdn_action* broadcast_actions;
};

struct lsdn_static_switch {
	struct lsdn_node node;
	struct switch_port *ports;

	/* The main ruleset that handles or non-broadcast packets
	 * (and broadcast packets too if broadcast handling is disabled */
	struct lsdn_ruleset forward_ruleset;

	int broadcast_enabled;
};

struct lsdn_static_switch *lsdn_static_switch_new (
		struct lsdn_network *net)
{
	struct lsdn_static_switch *sswitch = (struct lsdn_static_switch *)
			lsdn_node_new(net, &lsdn_static_switch_ops, sizeof(*sswitch));
	sswitch->broadcast_enabled = 1;

	lsdn_ruleset_init(&sswitch->forward_ruleset);
	lsdn_commit_to_network(&sswitch->node);
	return sswitch;
}

void lsdn_static_switch_enable_broadcast(
		struct lsdn_static_switch* sswitch,
		int enabled)
{
	sswitch->broadcast_enabled = enabled;
}

lsdn_err_t lsdn_static_switch_add_rule (
		const lsdn_mac_t *dst_mac,
		struct lsdn_port* port)
{
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(port->owner);

	// TODO: check if rule already exists

	// create the rule
	struct lsdn_rule *rule = malloc(sizeof(*rule));
	if (!rule)
		return LSDNE_NOMEM;

	lsdn_rule_init(rule);
	rule->action.id = LSDN_ACTION_PORT;
	rule->action.port = port;
	rule->target = LSDN_MATCH_DST_MAC;
	rule->contents.mac = *dst_mac;

	// add the rule to the ruleset
	lsdn_add_rule(&sswitch->forward_ruleset, rule, 0);
	return LSDNE_OK;
}

static lsdn_err_t create_broadcast_actions (
		struct lsdn_static_switch* sswitch,
		struct switch_port* p)
{
	struct lsdn_rule *r = &p->broadcast_rules[RULE_BROADCAST];
	size_t port_count = 0;
	lsdn_foreach_list(sswitch->node.ports, ports, struct lsdn_port, brtarget){
		port_count++;
	}

	free(p->broadcast_actions);
	/* port_count - 2 because one action is embedded in the rule and we need
	 * actions for all ports except the incoming one */
	p->broadcast_actions = calloc(sizeof(struct lsdn_action), port_count - 2);


	struct lsdn_action *last = NULL;
	struct lsdn_action *a = &r->action;

	lsdn_foreach_list(sswitch->node.ports, ports, struct lsdn_port, brtarget){
		if(brtarget == &p->port)
			continue;
		if(last)
			last->next = a;

		a->id = LSDN_ACTION_PORT;
		a->port = brtarget;

		last = a;
		if(a == &r->action)
			a = &p->broadcast_actions[0];
		else
			a++;
	}

	return LSDNE_OK;
}

static lsdn_err_t sswitch_update_rules (struct lsdn_node *node)
{	
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(node);

	lsdn_foreach_list(sswitch->node.ports, ports, struct lsdn_port, gen_port){
		struct switch_port* p = (struct switch_port*) gen_port;
		if(sswitch->broadcast_enabled) {
			lsdn_ruleset_init(&p->broadcast_ruleset);

			struct lsdn_rule *broadcast = &p->broadcast_rules[RULE_BROADCAST];
			struct lsdn_rule *other = &p->broadcast_rules[RULE_OTHER];

			lsdn_rule_init(broadcast);
			broadcast->target = LSDN_MATCH_DST_MAC;
			broadcast->mask.mac = lsdn_multicast_mac_mask;
			broadcast->contents.mac = lsdn_broadcast_mac;
			if(!p->broadcast_actions){
				int ret = create_broadcast_actions(sswitch, p);
				if(ret != LSDNE_OK)
					return ret;
			}

			lsdn_add_rule(&p->broadcast_ruleset, broadcast, 0);

			lsdn_rule_init(other);
			other->target = LSDN_MATCH_ANYTHING;
			other->action.id = LSDN_ACTION_RULE;
			other->action.rule = &sswitch->forward_ruleset;
			lsdn_add_rule(&p->broadcast_ruleset, other, 0);

			p->port.ruleset = &p->broadcast_ruleset;
		} else {
			p->port.ruleset = &sswitch->forward_ruleset;
		}
	}
	return LSDNE_OK;
}

static void free_port(struct lsdn_port* gen_port){
	struct switch_port* p = (struct switch_port*) gen_port;
	if(p->broadcast_actions){
		lsdn_rule_free(&p->broadcast_rules[0]);
		lsdn_rule_free(&p->broadcast_rules[1]);
		lsdn_ruleset_free(&p->broadcast_ruleset);
		free(p->broadcast_actions);
	}
}

static struct lsdn_port* new_port(struct lsdn_node *node, lsdn_port_type_t t){
	assert(t == LSDN_PORTT_DEFAULT);
	lsdn_as_static_switch(node);

	struct lsdn_port* p = (struct lsdn_port*) malloc(sizeof(struct switch_port));
	if(!p)
		return NULL;
	p->ops = &port_ops;
	return p;
}

static void free_sswitch (struct lsdn_node *node)
{
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(node);

	struct lsdn_rule *prev = lsdn_container_of(sswitch->forward_ruleset.rules.next,
			struct lsdn_rule,
			ruleset_list);
	struct lsdn_rule *cur = prev;
	while (!lsdn_is_list_empty(&sswitch->forward_ruleset.rules)) {
		cur = lsdn_container_of(cur->ruleset_list.next,
				struct lsdn_rule,
				ruleset_list);
		lsdn_rule_free(prev);
		free(prev); // TODO free prev in lsdn_rule_free ???
		prev = cur;
	}

	lsdn_ruleset_free(&sswitch->forward_ruleset);

	free(sswitch->ports);
}

static struct lsdn_port_ops port_ops = {
	.free = free_port
};

struct lsdn_node_ops lsdn_static_switch_ops = {
	.free_private_data = free_sswitch,
	.new_port = new_port,
	/* Sswitch updates its ruleset as the rules are added */
	.update_rules = sswitch_update_rules,
	/* Sswitch does not manage any linux interfaces */
	.update_ifs = lsdn_noop,
	.update_if_rules = lsdn_noop
};
