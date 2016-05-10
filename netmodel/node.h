#ifndef _LSDN_NODE_H
#define _LSDN_NODE_H

#include <stddef.h>
#include "network.h"

struct lsdn_node;
struct lsdn_node_ops;
struct lsdn_port;

#define LSDN_DEFINE_NODE(name) \
	struct lsdn_##name; \
	extern struct lsdn_node_ops lsdn_##name##_ops; \
	static inline struct lsdn_##name *lsdn_as_##name(struct lsdn_node *node) \
	{ \
		lsdn_check_cast(node, &lsdn_##name##_ops); \
		return (struct lsdn_##name *) node; \
	}

void lsdn_check_cast(struct lsdn_node *node, struct lsdn_node_ops *ops);
struct lsdn_port *lsdn_get_port(struct lsdn_node *node, size_t port);
size_t lsdn_get_port_count(struct lsdn_node *node);
void lsdn_node_free(struct lsdn_node *node);

#endif
