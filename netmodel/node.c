#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "private/node.h"
#include "private/tc.h"
#include "private/port.h"

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
	lsdn_list_init(&node->port_groups);
	lsdn_list_init(&node->ports);
	return node;
}


void lsdn_check_cast(struct lsdn_node *node, struct lsdn_node_ops *ops)
{
	assert(node->ops == ops);
}

void lsdn_node_free(struct lsdn_node *node){
	// TODO: free ports
	node->ops->free_private_data(node);

	// lsdn_list_remove(&node->ruleset_list); TODO
	lsdn_list_remove(&node->network_list);

	free(node);
}

struct lsdn_port *lsdn_create_port(struct lsdn_node *node, lsdn_port_type_t type){
	struct lsdn_port *p = node->ops->new_port(node, type);
	if(!p)
		return NULL;

	p->peer = NULL;
	p->owner = node;
	p->name = NULL;
	p->type = type;
	lsdn_list_init(&p->ports);
	lsdn_list_init(&p->class_ports);
	lsdn_list_add(&node->ports, &p->ports);
	return p;
}

struct lsdn_port_group *lsdn_create_port_group(
		struct lsdn_node *node, const char *name, lsdn_port_type_t type)
{
	struct lsdn_port_group* pg = malloc(sizeof(*pg));
	if(!pg)
		return NULL;
	pg->name = strdup(name);
	if(!pg->name){
		free(pg);
		return NULL;
	}
	pg->owner = node;
	pg->type = type;
	lsdn_list_init(&pg->port_groups);
	lsdn_list_add(&node->port_groups, &pg->port_groups);
	return pg;
}

lsdn_err_t lsdn_noop(struct lsdn_node *node){
	return LSDNE_OK;
}
