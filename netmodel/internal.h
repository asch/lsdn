#ifndef _LSDN_INTERNAL_H
#define _LSDN_INTERNAL_H

#include <stddef.h>
#include "errors.h"

struct lsdn_node;
struct lsdn_port;
struct lsdn_network;

struct lsdn_network{
        char* name;
        struct lsdn_node* head;
        struct lsdn_node* tail;
};

struct lsdn_node_ops
{
        void (*free_private_data)(struct lsdn_node *node);
        struct lsdn_port *(*get_port)(struct lsdn_node *node, size_t index);
        /* Creates/updates interfaces representing all ports owned by this node */
        lsdn_err_t (*update_ports)(struct lsdn_node *node);
        lsdn_err_t (*update_tc_rules)(struct lsdn_node *node);
};

struct lsdn_node
{
        struct lsdn_node_ops *ops;
        struct lsdn_network *network;
        /* Linked list of nodes in the whole network */
        struct lsdn_node *next;
        struct lsdn_node *previous;
        size_t port_count;
};

struct lsdn_port
{
        struct lsdn_port *peer;
        struct lsdn_node *owner;
        size_t index;
        char* ifname;
};

struct lsdn_node *lsdn_node_new(struct lsdn_network* net,
                                struct lsdn_node_ops* ops,
                                size_t size);
void lsdn_commit_to_network(struct lsdn_node* node);
void lsdn_port_init(struct lsdn_port* port,
                    struct lsdn_node *owner,
                    size_t index);

/* Creates all ports as dummy interfaces */
lsdn_err_t lsdn_update_ports_default(struct lsdn_node* node);
lsdn_err_t lsdn_free_default_ports(struct lsdn_node* node);

#endif
