#ifndef _LSDN_PORT_H
#define _LSDN_PORT_H

struct lsdn_port;
struct lsdn_port_group;
struct lsdn_node;
typedef int port_type_t;


#define LSDN_PORTT_DEFAULT 0

struct lsdn_port *lsdn_create_port_in_group(struct lsdn_port_group * group);
void lsdn_connect_ports(struct lsdn_port *a, struct lsdn_port *b);
void lsdn_connect(
	struct lsdn_node *a, struct lsdn_port **a_port,
	struct lsdn_node *b, struct lsdn_port **b_port);
void lsdn_port_free(struct lsdn_port *p);
void lsdn_port_set_name(struct lsdn_port *p, const char* name);
const char* lsdn_port_get_name(struct lsdn_port *p, const char* name);
port_type_t lsdn_port_type(struct lsdn_port *p);

#endif
