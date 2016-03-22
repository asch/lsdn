#ifndef _LSDN_INTERNAL_H
#define _LSDN_INTERNAL_H

#include <stddef.h>

struct lsdn_node;
struct lsdn_port;
struct lsdn_network;



struct lsdn_node_ops
{
        void (*free_private_data)(struct lsdn_node *node);
        struct lsdn_port *(*get_port)(struct lsdn_node *node, size_t index);
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
};

struct lsdn_node *lsdn_node_new(struct lsdn_network* net,
                                struct lsdn_node_ops* ops,
                                size_t size);
void lsdn_commit_to_network(struct lsdn_node* node);
void lsdn_port_init(struct lsdn_port* port,
                    struct lsdn_node *owner,
                    size_t index);

#endif
