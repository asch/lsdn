#include <assert.h>
#include "port.h"
#include "internal.h"

void lsdn_port_init(struct lsdn_port* port,
		struct lsdn_node* owner,
		size_t index, struct lsdn_ruleset *ruleset)
{
	port->index = index;
	port->owner = owner;
	port->peer = NULL;
	port->ruleset = ruleset;
}

void lsdn_connect(struct lsdn_port* a, struct lsdn_port* b)
{
	assert(a->peer == NULL && b->peer == NULL);
	a->peer = b;
	b->peer = a;
}
