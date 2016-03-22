#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "network.h"
#include "internal.h"

struct lsdn_node;

struct lsdn_network{
        char* name;
        struct lsdn_node* head;
        struct lsdn_node* tail;
};

struct lsdn_network *lsdn_network_new(const char* netname)
{
        struct lsdn_network* net = malloc(sizeof(*net));
        if(!net)
                return NULL;

        net->name = strdup(netname);
        if(!net->name)
                return NULL;

        return net;
}

void lsdn_commit_to_network(struct lsdn_node *node)
{
        assert(!node->previous && !node->next);
        struct lsdn_network* net = node->network;
        if(!net->head)
        {
                net->head = node;
                net->tail = node;
        } else {
                node->previous = net->tail;
                net->tail->next = node;
                net->tail = node;

        }
}

void lsdn_network_free(struct lsdn_network *network)
{
        if(!network)
                return;
        free(network->name);
        free(network);

        // TODO: release all nodes
}
