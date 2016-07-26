#ifndef _LSDN_PORT_H_PRIVATE_
#define _LSDN_PORT_H_PRIVATE_

#include <stddef.h>
#include "../include/port.h"

struct lsdn_node;
struct lsdn_ruleset;

struct lsdn_port {
	struct lsdn_port *peer;
	struct lsdn_node *owner;
	size_t index;
	struct lsdn_ruleset *ruleset;
};

void lsdn_port_init(struct lsdn_port *port,
		    struct lsdn_node *owner,
		    size_t index,
		    struct lsdn_ruleset *ruleset);

#endif
