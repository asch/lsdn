#include <stdlib.h>
#include <arpa/inet.h>
#include "include/static_switch.h"
#include "private/rule.h"
#include "private/tc.h"
#include "private/port.h"
#include "private/node.h"
#include "private/list.h"

enum {RULE_BROADCAST, RULE_OTHER};

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
	int broadcast_rules_created;
};

struct lsdn_static_switch *lsdn_static_switch_new (
		struct lsdn_network *net,
		size_t port_count)
{
	struct lsdn_static_switch *sswitch = (struct lsdn_static_switch *)
			lsdn_node_new(net, &lsdn_static_switch_ops, sizeof(*sswitch));
	sswitch->broadcast_enabled = 1;
	sswitch->broadcast_rules_created = 0;
	sswitch->node.port_count = port_count;
	sswitch->ports = calloc(port_count, sizeof(struct switch_port));
	if (!sswitch->ports) {
		lsdn_node_free(&sswitch->node);
		return NULL;
	}

	for (size_t i = 0;  i <port_count; i++) {
		lsdn_port_init(&sswitch->ports[i].port, &sswitch->node, i, NULL);
	}

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
		struct lsdn_static_switch *sswitch,
		const lsdn_mac_t *dst_mac,
		size_t port)
{
	// TODO: check if rule already exists

	// create the rule
	struct lsdn_rule *rule = malloc(sizeof(*rule));
	if (!rule)
		return LSDNE_NOMEM;

	lsdn_rule_init(rule);
	rule->action.id = LSDN_ACTION_PORT;
	rule->action.port = lsdn_get_port(&sswitch->node, port);
	rule->target = LSDN_MATCH_DST_MAC;
	rule->contents.mac = *dst_mac;

	// add the rule to the ruleset
	lsdn_add_rule(&sswitch->forward_ruleset, rule, 0);
	return LSDNE_OK;
}

static struct lsdn_port *get_sswitch_port (struct lsdn_node *node, size_t port)
{
	return &lsdn_as_static_switch(node)->ports[port].port;
}

static lsdn_err_t create_broadcast_actions (
		struct lsdn_static_switch* sswitch,
		struct switch_port* p)
{
	struct lsdn_rule *r = &p->broadcast_rules[RULE_BROADCAST];
	/* port_count - 2 because one action is embedded in the rule and we need
	 * actions for all ports except the incoming one */
	p->broadcast_actions = calloc(sizeof(struct lsdn_action), sswitch->node.port_count - 2);


	struct lsdn_action *last = NULL;
	struct lsdn_action *a = &r->action;
	for(size_t i = 0; i < sswitch->node.port_count; i++){
		if(i == p->port.index)
			continue;

		if(last)
			last->next = a;

		a->id = LSDN_ACTION_PORT;
		a->port = &sswitch->ports[i].port;

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
	if(sswitch->broadcast_rules_created)
		return LSDNE_OK;

	sswitch->broadcast_rules_created = 1;
	for(size_t i = 0; i < sswitch->node.port_count; i++) {
		struct switch_port* p = &sswitch->ports[i];
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

static void free_sswitch (struct lsdn_node *node)
{
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(node);

	for(size_t i = 0; i < sswitch->node.port_count; i++){
		struct switch_port *port = &sswitch->ports[i];
		if(sswitch->broadcast_rules_created) {
			lsdn_rule_free(&port->broadcast_rules[0]);
			lsdn_rule_free(&port->broadcast_rules[1]);
			lsdn_ruleset_free(&port->broadcast_ruleset);
			free(port->broadcast_actions);
		}
	}

	lsdn_foreach_list(sswitch->forward_ruleset.rules, ruleset_list, struct lsdn_rule, r) {
		lsdn_rule_free(r);
		free(r);
	}

	lsdn_ruleset_free(&sswitch->forward_ruleset);

	free(sswitch->ports);
}

struct lsdn_node_ops lsdn_static_switch_ops = {
	.free_private_data = free_sswitch,
	.get_port = get_sswitch_port,
	/* Sswitch updates its ruleset as the rules are added */
	.update_rules = sswitch_update_rules,
	/* Sswitch does not manage any linux interfaces */
	.update_ifs = lsdn_noop,
	.update_if_rules = lsdn_noop
};
