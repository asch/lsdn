#ifndef _LSDN_INTERNAL_H
#define _LSDN_INTERNAL_H

#include <stddef.h>
#include "errors.h"
#include "list.h"

struct lsdn_node;
struct lsdn_port;
struct lsdn_network;
struct lsdn_ruleset;

struct lsdn_network {
	char *name;
	/* Last used unique id for interface names etc */
	int unique_id;
	struct lsdn_list_entry nodes;
};

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
};

struct lsdn_port {
	struct lsdn_port *peer;
	struct lsdn_node *owner;
	size_t index;
	struct lsdn_ruleset *ruleset;
};

/* Linux interface managed by the network, used for deciding the packet
 * fates at a particular point. They back some of the rulesets (currently all)
 */
struct lsdn_if{
	char *ifname;
};

struct lsdn_node *lsdn_node_new(struct lsdn_network *net,
				struct lsdn_node_ops *ops,
				size_t size);
void lsdn_commit_to_network(struct lsdn_node *node);
void lsdn_port_init(struct lsdn_port *port,
		    struct lsdn_node *owner,
		    size_t index,
		    struct lsdn_ruleset *ruleset);


#endif
