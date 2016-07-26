#ifndef _LSDN_NODE_H_PRIVATE_
#define _LSDN_NODE_H_PRIVATE_

#include <stddef.h>
#include "../include/node.h"
#include "../include/errors.h"
#include "list.h"

struct lsdn_node;

struct lsdn_node_ops {
	void (*free_private_data)(struct lsdn_node *node);
	struct lsdn_port *(*get_port)(struct lsdn_node *node, size_t index);

	/* Update/create all rulesets on the lsdn_ports managed by this node */
	lsdn_err_t (*update_rules)(struct lsdn_node *node);
	/* Update/create all linux interfaces managed by this node */
	lsdn_err_t (*update_ifs)(struct lsdn_node *node);
	/* Update/create all tc rules on linux interfaces managed by this node */
	lsdn_err_t (*update_if_rules)(struct lsdn_node *node);
};
lsdn_err_t lsdn_noop(struct lsdn_node *node);

struct lsdn_node {
	struct lsdn_node_ops *ops;
	struct lsdn_network *network;
	size_t port_count;
	struct lsdn_list_entry network_list;
	struct lsdn_list_entry ruleset_list;
};

struct lsdn_node *lsdn_node_new(struct lsdn_network *net,
				struct lsdn_node_ops *ops,
				size_t size);
void lsdn_commit_to_network(struct lsdn_node *node);

#endif
