#include <stdlib.h>
#include <arpa/inet.h>
#include "static_switch.h"
#include "internal.h"
#include "tc.h"

struct lsdn_static_switch {
	struct lsdn_node node;
	struct lsdn_port* ports;
	struct rule* first_rule;
};

struct rule {
	lsdn_mac_t mac;
	size_t port;
	struct rule* next;
};

struct lsdn_static_switch *lsdn_static_switch_new(
		struct lsdn_network* net,
		size_t port_count)
{
	struct lsdn_static_switch* sswitch = (struct lsdn_static_switch*)
			lsdn_node_new(net, &lsdn_static_switch_ops, sizeof(*sswitch));
	sswitch->node.port_count = port_count;
	sswitch->ports = (struct lsdn_port*) calloc(port_count, sizeof(struct lsdn_port));
	sswitch->first_rule = NULL;
	if (!sswitch->ports) {
		lsdn_node_free(&sswitch->node);
		return NULL;
	}
     
	for (size_t i = 0; i<port_count; i++) {
		lsdn_port_init(&sswitch->ports[i], &sswitch->node, i);
	}

	lsdn_commit_to_network(&sswitch->node);
	return sswitch;
}

lsdn_err_t lsdn_static_switch_add_rule(
		struct lsdn_static_switch *sswitch,
		const lsdn_mac_t* dst_mac,
		size_t port)
{
	// TODO: check if rule already exists
	struct rule* rule = malloc(sizeof(*rule));
	if (!rule)
		return LSDNE_NOMEM;
	rule->mac = *dst_mac;
	rule->port = port;
	rule->next = sswitch->first_rule;
	sswitch->first_rule = rule;
	return LSDNE_OK;
}

static struct lsdn_port* get_sswitch_port(struct lsdn_node* node, size_t port)
{
	return &lsdn_as_static_switch(node)->ports[port];
}

static void free_sswitch(struct lsdn_node* node)
{
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(node);
	lsdn_free_default_ports(node);
	free(sswitch->ports);
	struct rule* rule = sswitch->first_rule;
	while (rule) {
		struct rule* next = rule->next;
		free(rule);
		rule = next;
	}
}
static void mkrule(struct lsdn_static_switch* sswitch, struct rule *rule, struct lsdn_port* port)
{
	runcmd("tc filter add dev %s parent 1: protocol all "
	       "u32 match u32 0x%x at -12 "
	       "action mirred egress redirect dev %s",
	       port->ifname, ntohl(rule->mac.high_32),
	       sswitch->ports[rule->port].peer->ifname);
}
static lsdn_err_t update_tc(struct lsdn_node *node)
{
	struct lsdn_static_switch *sswitch = lsdn_as_static_switch(node);
	for (size_t i = 0; i < node->port_count; i++) {
		for (struct rule *r = sswitch->first_rule; r; r = r->next) {
			mkrule(sswitch, r, &sswitch->ports[i]);
		}
	}
	return LSDNE_OK;
}

struct lsdn_node_ops lsdn_static_switch_ops = {
	.free_private_data = free_sswitch,
	.get_port = get_sswitch_port,
	.update_ports = lsdn_update_ports_default,
	.update_tc_rules = update_tc
};
