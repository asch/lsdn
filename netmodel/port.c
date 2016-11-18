#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "private/port.h"
#include "include/node.h"

void lsdn_connect_ports(struct lsdn_port *a, struct lsdn_port *b)
{
	assert(a->peer == NULL && b->peer == NULL);
	a->peer = b;
	b->peer = a;
}

struct lsdn_port *lsdn_create_port_in_group(struct lsdn_port_group * group)
{
	return lsdn_create_port(group->owner, group->type);
}

void lsdn_connect(
	struct lsdn_node *a, struct lsdn_port **a_port_dst,
	struct lsdn_node *b, struct lsdn_port **b_port_dst)
{
	struct lsdn_port* a_port = lsdn_create_port(a, LSDN_PORTT_DEFAULT);
	struct lsdn_port* b_port = lsdn_create_port(b, LSDN_PORTT_DEFAULT);
	if(!(a_port && b_port)){
		lsdn_port_free(a_port);
		lsdn_port_free(b_port);
		return;
	}
	lsdn_connect_ports(a_port, b_port);
	if(a_port_dst)
		*a_port_dst = a_port;
	if(b_port_dst)
		*b_port_dst = b_port;
}

void lsdn_port_free(struct lsdn_port *p){
	if(!p)
		return;
	p->ops->free(p);
	lsdn_list_remove(&p->ports);
	lsdn_list_remove(&p->class_ports);
	free(p->name);
	free(p);
}

void lsdn_port_set_name(struct lsdn_port *p, const char* name){
	/* TODO: Check name uniqueness */
	free(p->name);
	p->name = strdup(name);
}

const char* lsdn_port_get_name(struct lsdn_port *p, const char* name){
	return p->name;
}

lsdn_port_type_t lsdn_port_type(struct lsdn_port *p){
	return p->type;
}
