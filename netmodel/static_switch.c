#include <stdlib.h>
#include <arpa/inet.h>
#include "static_switch.h"
#include "internal.h"
#include "rule.h"
#include "tc.h"

struct lsdn_static_switch {
	struct lsdn_node node;
	struct lsdn_port *ports;
	struct lsdn_ruleset rules;
	struct rule *rule_list;
};

struct rule{
	struct lsdn_rule lsdn;
	struct rule *next;
};


struct lsdn_static_switch *lsdn_static_switch_new(
		struct lsdn_network *net,
		size_t port_count)
{
	struct lsdn_static_switch *sswitch = (struct lsdn_static_switch *)
			lsdn_node_new(net, &lsdn_static_switch_ops, sizeof(*sswitch));
	sswitch->node.port_count = port_count;
	sswitch->ports = (struct lsdn_port *) calloc(port_count, sizeof(struct lsdn_port));
	if (!sswitch->ports) {
		lsdn_node_free(&sswitch->node);
		return NULL;
	}
     
	// create the ports and initialize them all with the same ruleset
	lsdn_ruleset_init(&sswitch->rules);
	for (size_t i = 0; i<port_count; i++) {
		lsdn_port_init(&sswitch->ports[i], &sswitch->node, i, &sswitch->rules);
		sswitch->ports[i].ruleset = &sswitch->rules;
	}
	lsdn_commit_to_network(&sswitch->node);
	return sswitch;
}

lsdn_err_t lsdn_static_switch_add_rule(
		struct lsdn_static_switch *sswitch,
		const lsdn_mac_t *dst_mac,
		size_t port)
{
	// TODO: check if rule already exists

	// create the rule
	struct rule *rule = malloc(sizeof(*rule));
	if (!rule)
		return LSDNE_NOMEM;
	lsdn_rule_init(&rule->lsdn);
	rule->lsdn.action.id = LSDN_ACTION_PORT;
	rule->lsdn.action.port = lsdn_get_port(&sswitch->node, port);
	rule->lsdn.target = LSDN_MATCH_DST_MAC;
	rule->lsdn.contents.mac = *dst_mac;

	// add the rule to the ruleset and also our internal list of rules (for deallocation)
	lsdn_add_rule(&sswitch->rules, &rule->lsdn, 0);
	rule->next = sswitch->rule_list;
	sswitch->rule_list = rule;
	return LSDNE_OK;
}

static struct lsdn_port *get_sswitch_port(struct lsdn_node *node, size_t port)
{
	return &lsdn_as_static_switch(node)->ports[port];
}

static void free_sswitch(struct lsdn_node *node)
{
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(node);
	free(sswitch->ports);
	struct rule *rule = sswitch->rule_list;
	while (rule) {
		struct rule *next = rule->next;
		free(rule);
		rule = next;
	}
}

struct lsdn_node_ops lsdn_static_switch_ops = {
	.free_private_data = free_sswitch,
	.get_port = get_sswitch_port,
	/* Sswitch updates its ruleset as the rules are added */
	.update_rules = lsdn_noop,
	/* Sswitch does not manage any linux interfaces */
	.update_ifs = lsdn_noop,
	.update_if_rules = lsdn_noop
};
