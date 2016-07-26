#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "private/network.h"
#include "private/list.h"
#include "private/node.h"
#include "private/port.h"
#include "private/rule.h"
#include "private/nl.h"
#include "private/tc.h"

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

static lsdn_err_t create_if_for_ruleset(
		struct lsdn_network *network,
		struct lsdn_node* owner,
		struct lsdn_ruleset *ruleset)
{
	// TODO: handle updating
	if(lsdn_if_created(&ruleset->interface))
		return LSDNE_OK;

	ruleset->if_rules_created = 0;

	size_t maxname = LSDN_MAX_IF_SUFFIX + strlen(network->name);
	char *namebuf = (char *) malloc(maxname);
	if(!namebuf) {
		return LSDNE_NOMEM;
	}
	snprintf(namebuf, maxname, "%s-%d", network->name, ++network->unique_id);

	struct mnl_socket *sock = lsdn_socket_init();
	if (sock == NULL) {
		free(namebuf);
		return LSDNE_BAD_SOCK;
	}

	int err = lsdn_link_dummy_create(sock, &ruleset->interface, namebuf);
	printf("Creating interface '%s': %s\n", namebuf, strerror(-err));

	free(namebuf);

	if (err != 0){
		return LSDNE_FAIL;
	}

	err = lsdn_qdisc_htb_create(sock, ruleset->interface.ifindex,
			TC_H_ROOT, LSDN_DEFAULT_EGRESS_HANDLE, 10, 0);
	printf("Creating HTB qdisc for interface '%s': %s\n", ruleset->interface.ifname, strerror(-err));
	if (err != 0){
		return LSDNE_FAIL;
	}

	err = lsdn_link_set(sock, ruleset->interface.ifindex, true);
	printf("Setting interface '%s' up: %s\n", ruleset->interface.ifname, strerror(-err));
	if (err != 0){
		return LSDNE_FAIL;
	}

	mnl_socket_close(sock);

	lsdn_list_add(&owner->ruleset_list, &ruleset->node_rules);

	lsdn_foreach_list(ruleset->rules, ruleset_list, struct lsdn_rule, r) {
		for(struct lsdn_action* a = &r->action; a; a = a->next) {
			if(a->id == LSDN_ACTION_RULE) {
				lsdn_err_t rv = create_if_for_ruleset(network, owner, a->rule);
				if(rv != LSDNE_OK)
					return rv;
			}
		}
	}

	return LSDNE_OK;
}



static lsdn_err_t create_if_rules_for_ruleset(
		struct lsdn_network *network,
		struct lsdn_ruleset *ruleset)
{
	if(ruleset->if_rules_created)
		return LSDNE_OK;
	ruleset->if_rules_created = 1;

	// TODO: move it to rules.c when we have folding and know what should be done
	// TODO: support flower -- in my tc, I have no support for it, even though
	//       I have the kernel module
	int order = 1;
	lsdn_foreach_list(ruleset->rules, ruleset_list, struct lsdn_rule, r) {
		printf("Creating rules for %s (%d)\n",
		       ruleset->interface.ifname,
		       ruleset->interface.ifindex);

		struct lsdn_filter *filter = lsdn_filter_init(
					"flower",
					ruleset->interface.ifindex,
					order,
					LSDN_DEFAULT_EGRESS_HANDLE,
					order,
					ETH_P_ALL << 8/* magic trick 1 */);

		order++;

		switch (r->target) {
		case LSDN_MATCH_ANYTHING:
			break;
		case LSDN_MATCH_ETHERTYPE:
			printf("When ethertype matches\n");
			lsdn_flower_set_eth_type(filter, r->contents.ethertype); // TODO add r->mask.ethertype
			break;
		case LSDN_MATCH_SRC_MAC:
			printf("When src mac matches\n");
			lsdn_flower_set_src_mac(
					filter, (char *) r->contents.mac.bytes,
					(char *) r->mask.mac.bytes);
			break;
		case LSDN_MATCH_DST_MAC:
			printf("When dst mac matches\n");
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
	/* 1) Let the nodes update the rule definitions */
	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		rv = n->ops->update_rules(n);
		if(rv != LSDNE_OK)
			return rv;
	}

	/* 2) Update all interfaces managed by nodes */
	/* 3) Collect all reachable rule definitions and create their backing interfaces*/
	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		rv = n->ops->update_ifs(n);
		if(rv != LSDNE_OK)
			return rv;

		for(size_t i = 0; i < n->port_count; i++) {
			rv = create_if_for_ruleset(network, n, lsdn_get_port(n, i)->ruleset);
			if(rv != LSDNE_OK)
				return rv;
		}
	}
	/* 4) Set filters on all interfaces owned by rulesets */
	/* 5) Set filters and other settings on all interfaces owned by nodes */
	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {

		lsdn_foreach_list(n->ruleset_list, node_rules, struct lsdn_ruleset, r) {
			rv = create_if_rules_for_ruleset(network, r);
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
