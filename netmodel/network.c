#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "network.h"
#include "internal.h"

struct lsdn_node;


struct lsdn_network *lsdn_network_new(const char* netname)
{
	struct lsdn_network* net = malloc(sizeof(*net));
	if (!net)
		return NULL;

	lsdn_list_init(&net->nodes);
	net->name = strdup(netname);
	if (!net->name)
		return NULL;


	return net;
}

void lsdn_commit_to_network(struct lsdn_node *node)
{
	struct lsdn_network* net = node->network;
	lsdn_list_add(&net->nodes, &node->network_list);
}

void lsdn_network_create(struct lsdn_network *network){
	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		n->ops->update_ports(n);
	}

	lsdn_foreach_list(network->nodes, network_list, struct lsdn_node, n) {
		n->ops->update_tc_rules(n);
	}
}

void lsdn_network_free(struct lsdn_network *network)
{
	if (!network)
		return;
	free(network->name);
	free(network);

	// TODO: release all nodes
}
