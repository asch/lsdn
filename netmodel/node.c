#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "node.h"
#include "internal.h"
#include "tc.h"

struct lsdn_node *lsdn_node_new(
		struct lsdn_network *net,
		struct lsdn_node_ops *ops,
		size_t size)
{
	struct lsdn_node *node = malloc(size);
	if (!node)
		return NULL;

	node->network = net;
	node->ops = ops;
	lsdn_list_init(&node->network_list);
	lsdn_list_init(&node->ruleset_list);
	return node;
}

struct lsdn_port *lsdn_get_port(struct lsdn_node *node, size_t port)
{
	assert(port < node->port_count);
	return node->ops->get_port(node, port);
}

size_t lsdn_get_port_count(struct lsdn_node *node)
{
	return node->port_count;
}

void lsdn_check_cast(struct lsdn_node *node, struct lsdn_node_ops *ops)
{
	assert(node->ops == ops);
}

void lsdn_node_free(struct lsdn_node *node){
	// TODO: remove from network, free memory, call free callback
}
lsdn_err_t lsdn_noop(struct lsdn_node *node){
	return LSDNE_OK;
}
