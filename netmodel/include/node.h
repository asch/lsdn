#ifndef _LSDN_NODE_H
#define _LSDN_NODE_H

#include <stddef.h>
#include "network.h"
#include "port.h"

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

struct lsdn_port *lsdn_create_port(struct lsdn_node *node, lsdn_port_type_t type);
struct lsdn_port_group *lsdn_create_port_group(
		struct lsdn_node *node, const char *name, lsdn_port_type_t type);
struct lsdn_port_group *lsdn_port_group_by_name(
		struct lsdn_node *node, const char *name);
struct lsdn_port *lsdn_port_by_name(struct lsdn_node *node, const char* name);

void lsdn_check_cast(struct lsdn_node *node, struct lsdn_node_ops *ops);
void lsdn_node_free(struct lsdn_node *node);

#endif
