#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "node.h"
#include "internal.h"
#include "tc.h"

struct lsdn_node *lsdn_node_new(
                struct lsdn_network* net,
                struct lsdn_node_ops* ops,
                size_t size)
{
        struct lsdn_node * node = malloc(size);
        if(!node)
                return NULL;

        node->network = net;
        node->ops = ops;
        node->next = NULL;
        node->previous = NULL;
        return node;
}
struct lsdn_port* lsdn_get_port(struct lsdn_node *node, size_t port)
{
        return node->ops->get_port(node, port);
}
size_t lsdn_get_port_count(struct lsdn_node *node)
{
        return node->port_count;
}
void lsdn_check_cast(struct lsdn_node* node, struct lsdn_node_ops* ops)
{
        assert(node->ops == ops);
}
lsdn_err_t lsdn_free_default_ports(struct lsdn_node *node)
{
        for(size_t i = 0; i < node->port_count; i++)
        {
                struct lsdn_port* port = lsdn_get_port(node, i);
                if(port->ifname)
                {
                        runcmd("ip link delete %s", port->ifname);
                        free(port->ifname);
                        port->ifname = NULL;
                }
        }
        return LSDNE_OK;
}
lsdn_err_t lsdn_update_ports_default(struct lsdn_node *node)
{
        // TODO: better naming of ports
        static int portseq = 1;
        for(size_t i = 0; i < node->port_count; i++)
        {
                struct lsdn_port* port = lsdn_get_port(node, i);
                if(port->ifname)
                        continue;

                size_t maxname = 20 + strlen(node->network->name);
                port->ifname = malloc(maxname);
                if(!port->ifname)
                        // the partial result should not cause any problems here
                        return LSDNE_NOMEM;
                snprintf(port->ifname, maxname, "%s-%d", node->network->name, portseq++);
                runcmd("ip link add name %s type dummy", port->ifname);
        }
        return LSDNE_OK;
}

void lsdn_node_free(struct lsdn_node* node){
        // TODO: remove from network, free memory, call free callback
}
