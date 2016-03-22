#include <stdlib.h>
#include <string.h>
#include "netdev.h"
#include "internal.h"


struct lsdn_netdev
{
        struct lsdn_node node;
        char* linux_if;
        struct lsdn_port port;
};

struct lsdn_netdev* lsdn_netdev_new(
                struct lsdn_network* net,
                const char* linux_if)
{
        struct lsdn_netdev* netdev = (struct lsdn_netdev*)
                        lsdn_node_new(net, &lsdn_netdev_ops, sizeof(*netdev));
        netdev->linux_if = strdup(linux_if);
        lsdn_port_init(&netdev->port, &netdev->node, 0);
        lsdn_commit_to_network(&netdev->node);
        return netdev;
}

static struct lsdn_port *get_netdev_port(struct lsdn_node* node, size_t index)
{
        return &lsdn_as_netdev(node)->port;
}

static void free_netdev(struct lsdn_node *node)
{
        free(lsdn_as_netdev(node)->linux_if);
}

struct lsdn_node_ops lsdn_netdev_ops =
{
        free_netdev,
        get_netdev_port,
};
