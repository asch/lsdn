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
#include "nl.h"

struct lsdn_node;


struct lsdn_network *lsdn_network_new(const char *netname)
{
	struct lsdn_network *net = malloc(sizeof(*net));
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
	struct lsdn_network *net = node->network;
	lsdn_list_add(&net->nodes, &node->network_list);
}

static lsdn_err_t create_if_for_ruleset(struct lsdn_network *network, struct lsdn_ruleset *ruleset)
{
	// TODO: handle updating
	if(ruleset->interface)
		return LSDNE_OK;

	ruleset->if_rules_created = 0;
	size_t maxname = 20 + strlen(network->name);
	ruleset->interface = (struct lsdn_if *) malloc(sizeof(struct lsdn_if));
	if(!ruleset->interface)
		return LSDNE_NOMEM;
	char *ifname = ruleset->interface->ifname = (char *) malloc(maxname);
	if(!ruleset->interface->ifname) {
		free(ruleset->interface);
		ruleset->interface = NULL;
		return LSDNE_NOMEM;
	}

	snprintf(ifname, maxname, "%s-%d", network->name, ++network->unique_id);

	struct mnl_socket *sock = lsdn_socket_init();
	if (sock == NULL) {
		return LSDNE_BAD_SOCK;
	}

	int err = lsdn_link_dummy_create(sock, ifname);
	printf("Creating interface '%s': %s\n", ifname, strerror(-err));
	if (err != 0){
		return LSDNE_FAIL;
	}

	unsigned int ifindex = if_nametoindex(ifname);
	ruleset->interface->ifindex = ifindex;

	err = lsdn_qdisc_htb_create(sock, ifindex,
			TC_H_ROOT, LSDN_DEFAULT_EGRESS_HANDLE, 10, 0);
	printf("Creating HTB qdisc for interface '%s': %s\n", ifname, strerror(-err));
	if (err != 0){
		return LSDNE_FAIL;
	}

	err = lsdn_link_set(sock, ifname, true);
	printf("Setting interface '%s' up: %s\n", ifname, strerror(-err));
	if (err != 0){
		return LSDNE_FAIL;
	}

	mnl_socket_close(sock);

	return LSDNE_OK;
}



static lsdn_err_t create_if_rules_for_ruleset(
		struct lsdn_network *network,
		struct lsdn_ruleset *ruleset)
{
	if(ruleset->if_rules_created)
		return LSDNE_OK;
	ruleset->if_rules_created = 1;

	const char *ifname = ruleset->interface->ifname;
	// TODO: move it to rules.c when we have folding and know what should be done
	// TODO: support flower -- in my tc, I have no support for it, even though
	//       I have the kernel module
	int handle = 1;
	lsdn_foreach_list(ruleset->rules, ruleset_list, struct lsdn_rule, r) {
		unsigned int if_index = if_nametoindex(ifname);
		printf("Creating rules for %s (%d)\n", ifname, if_index);

		struct lsdn_filter *filter = lsdn_filter_init(
					"flower",
					if_index,
					handle++,
					LSDN_DEFAULT_EGRESS_HANDLE,
					LSDN_DEFAULT_PRIORITY,
					ETH_P_ALL << 8/* magic trick 1 */);

		switch (r->target) {
		case LSDN_MATCH_ANYTHING:
			break;
		case LSDN_MATCH_ETHERTYPE:
			lsdn_flower_set_eth_type(filter, r->contents.ethertype); // TODO add r->mask.ethertype
			break;
		case LSDN_MATCH_SRC_MAC:
			lsdn_flower_set_src_mac(
					filter, (char *) r->contents.mac.bytes,
					(char *) r->mask.mac.bytes);
			break;
		case LSDN_MATCH_DST_MAC:
			lsdn_flower_set_dst_mac(
					filter, (char *) r->contents.mac.bytes,
					(char *) r->mask.mac.bytes);
			break;
		}

		lsdn_filter_actions_start(filter, TCA_FLOWER_ACT);

		actions_for(&r->action, filter);

		lsdn_filter_actions_end(filter);

		struct mnl_socket *sock = lsdn_socket_init();

		int err = lsdn_filter_create(sock, filter);

		printf("network.c err = %d\n", err);

		// TODO: check err and return some errorcode

		lsdn_socket_free(sock);
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
