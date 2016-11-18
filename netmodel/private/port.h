#ifndef _LSDN_PORT_H_PRIVATE_
#define _LSDN_PORT_H_PRIVATE_

#include <stddef.h>
#include "../include/port.h"
#include "list.h"

struct lsdn_node;
struct lsdn_ruleset;

struct lsdn_port_ops {
	void (*free)(struct lsdn_port*);
};

struct lsdn_port_group{
	char *name;
	struct lsdn_node *owner;
	lsdn_port_type_t type;
	struct lsdn_list_entry port_groups;
};

struct lsdn_port {
	struct lsdn_port *peer;
	struct lsdn_node *owner;
	struct lsdn_ruleset *ruleset;
	char *name;
	lsdn_port_type_t type;
	struct lsdn_port_ops *ops;

	struct lsdn_list_entry ports;
	struct lsdn_list_entry class_ports;
};

#endif
