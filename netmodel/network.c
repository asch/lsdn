#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "network.h"
#include "internal.h"
#include "node.h"
#include "tc.h"
#include "rule.h"
#include "nettypes.h"

struct lsdn_node;


struct lsdn_network *lsdn_network_new(const char* netname)
{
	struct lsdn_network* net = malloc(sizeof(*net));
	if (!net)
		return NULL;

	lsdn_list_init(&net->nodes);

	net->unique_id = 0; /* todo */

	net->name = strdup(netname);
	if (!net->name) {
		free(net);
		return NULL;
	}

	return net;
}

void lsdn_commit_to_network(struct lsdn_node *node)
{
	struct lsdn_network* net = node->network;
	lsdn_list_add(&net->nodes, &node->network_list);
}

static lsdn_err_t create_if_for_ruleset(struct lsdn_network* network, struct lsdn_ruleset* ruleset)
{
	// TODO: handle updating
	if(ruleset->interface)
		return LSDNE_OK;

	ruleset->if_rules_created = 0;
	size_t maxname = 20 + strlen(network->name);
	ruleset->interface = (struct lsdn_if*) malloc(sizeof(struct lsdn_if));
	if(!ruleset->interface)
		return LSDNE_NOMEM;
	char* ifname = ruleset->interface->ifname = (char*) malloc(maxname);
	if(!ruleset->interface->ifname) {
		free(ruleset->interface);
		ruleset->interface = NULL;
		return LSDNE_NOMEM;
	}

	snprintf(ifname, maxname, "%s-%d", network->name, ++network->unique_id);
	runcmd("ip link add name %s type dummy", ifname);
	runcmd("tc qdisc add dev %s root handle 1: htb", ifname);
	runcmd("ip link set %s up", ifname);

	return LSDNE_OK;
}
static lsdn_err_t create_if_rules_for_ruleset(
		struct lsdn_network* network,
		struct lsdn_ruleset* ruleset)
{
	if(ruleset->if_rules_created)
		return LSDNE_OK;
	ruleset->if_rules_created = 1;

	const char* ifname = ruleset->interface->ifname;
	// TODO: move it to rules.c when we have folding and know what should be done
	// TODO: support flower -- in my tc, I have no support for it, even though
	//       I have the kernel module
	lsdn_foreach_list(ruleset->rules, ruleset_list, struct lsdn_rule, r) {
		char addrbuf[LSDN_MAC_STRING_LEN + 1];
		switch(r->target) {
		case LSDN_MATCH_ANYTHING:
			runcmd("tc filter add dev %s parent 1: protocol all "
			       "u32 match u32 0 0 %s",
			       ifname, actions_for(&r->action));
			break;
		case LSDN_MATCH_ETHERTYPE:
			runcmd("tc filter add dev %s parent 1: protocol all "
			       "u16 0x%x 0x%x at -2 %s",
			       ifname, r->contents.ethertype, r->mask.ethertype);
			break;
		case LSDN_MATCH_SRC_MAC:
			lsdn_mac_to_string(&r->contents.mac, addrbuf);
			runcmd("tc filter add dev %s parent 1: protocol all "
			       "u32 match u32 0x%x 0x%x at -6 match u16 0x%x 0x%x at -8 %s",
			       ifname,
			       lsdn_mac_low32(&r->contents.mac), lsdn_mac_low32(&r->mask.mac),
			       lsdn_mac_high16(&r->contents.mac), lsdn_mac_high16(&r->mask.mac),
			       actions_for(&r->action));
			break;
		case LSDN_MATCH_DST_MAC:
			lsdn_mac_to_string(&r->contents.mac, addrbuf);
			runcmd("tc filter add dev %s parent 1: protocol all "
			       "u32 match u32 0x%x 0x%x at -12 match u16 0x%x 0x%x at -14 %s",
			       ifname,
			       lsdn_mac_low32(&r->contents.mac), lsdn_mac_low32(&r->mask.mac),
			       lsdn_mac_high16(&r->contents.mac), lsdn_mac_high16(&r->mask.mac),
			       actions_for(&r->action));
			break;
		}
	}

	return LSDNE_OK;
}

lsdn_err_t lsdn_network_create(struct lsdn_network *network)
{
	lsdn_err_t rv;
	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		rv = n->ops->update_rules(n);
		if(rv != LSDNE_OK)
			return rv;
	}

	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		for(size_t i = 0; i < n->port_count; i++) {
			rv = create_if_for_ruleset(network, lsdn_get_port(n, i)->ruleset);
			if(rv != LSDNE_OK)
				return rv;
		}
		rv = n->ops->update_ifs(n);
		if(rv != LSDNE_OK)
			return rv;
	}

	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		for(size_t i = 0; i < n->port_count; i++) {
			rv = create_if_rules_for_ruleset(network, lsdn_get_port(n, i)->ruleset);
			if(rv != LSDNE_OK)
				return rv;
		}
		rv = n->ops->update_if_rules(n);
		if(rv != LSDNE_OK)
			return rv;
	}
	return LSDNE_OK;
}

void lsdn_network_free(struct lsdn_network *network)
{
	if (!network)
		return;
	free(network->name);
	free(network);

	// TODO: release all nodes and interfaces created for rulesets
}
