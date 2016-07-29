#include <stdlib.h>
#include <string.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>
#include "include/netdev.h"
#include "include/nettypes.h"
#include "private/tc.h"
#include "private/rule.h"
#include "include/util.h"
#include "private/nl.h"
#include "private/node.h"
#include "private/port.h"


struct lsdn_netdev {
	struct lsdn_node node;
	struct lsdn_port port;
	struct lsdn_ruleset rules;
	struct lsdn_rule default_rule;
	char *linux_if;
};

struct lsdn_netdev *lsdn_netdev_new(
		struct lsdn_network *net,
		const char *linux_if)
{
	char *ifname_copy = strdup(linux_if);
	if(!ifname_copy)
		return NULL;

	struct lsdn_netdev *netdev = lsdn_as_netdev(
			lsdn_node_new(net, &lsdn_netdev_ops, sizeof(*netdev)));
	if(!netdev) {
		free(ifname_copy);
		return NULL;
	}
	netdev->linux_if = ifname_copy;
	netdev->node.port_count = 1;
	lsdn_port_init(&netdev->port, &netdev->node, 0, &netdev->rules);
	lsdn_ruleset_init(&netdev->rules);
	lsdn_rule_init(&netdev->default_rule);
	lsdn_add_rule(&netdev->rules, &netdev->default_rule, 0);
	netdev->default_rule.action.id = LSDN_ACTION_IF;
	netdev->default_rule.action.ifname = netdev->linux_if;

	lsdn_commit_to_network(&netdev->node);
	return netdev;
}

static struct lsdn_port *get_netdev_port(struct lsdn_node *node, size_t index)
{
	UNUSED(index);
	return &lsdn_as_netdev(node)->port;
}

static void free_netdev(struct lsdn_node *node)
{
	free(lsdn_as_netdev(node)->linux_if);
}

static lsdn_err_t update_if_rules(struct lsdn_node *node)
{
	// TODO: delete the old rules /  do not duplicate the rules

	//runcmd("tc qdisc add dev %s handle ffff: ingress", netdev->linux_if);

	struct lsdn_netdev *netdev = lsdn_as_netdev(node);
	printf("Creating rules for netdev %s\n", netdev->linux_if);
	unsigned int if_index = if_nametoindex(netdev->linux_if);
	struct mnl_socket *sock = lsdn_socket_init();
	lsdn_qdisc_ingress_create(sock, if_index);

	//runcmd("tc filter add dev %s parent ffff: protocol all u32 match "
	//"u32 0 0 action mirred egress redirect dev %s",
	//netdev->linux_if, netdev->port.peer->ruleset->interface->ifname);

	struct lsdn_filter *filter = lsdn_filter_init("flower", if_index,
						      1,
						      LSDN_INGRESS_HANDLE,
						      LSDN_DEFAULT_PRIORITY,
						      ETH_P_ALL<<8 /* magic trick 1 */);

	lsdn_filter_actions_start(filter, TCA_FLOWER_ACT);

	struct lsdn_if* dst_if = &netdev->port.peer->ruleset->interface;
	assert(lsdn_if_created(dst_if));
	unsigned int ifindex_dst = dst_if->ifindex;

	lsdn_action_mirred_add(filter, 1, TC_ACT_STOLEN, TCA_EGRESS_REDIR,
			       ifindex_dst);

	lsdn_filter_actions_end(filter);

	int err = lsdn_filter_create(sock, filter);

	// TODO: check err and return some errorcode
	printf("netdev.c err = %d\n", err);

	lsdn_filter_free(filter);

	lsdn_socket_free(sock);
	return LSDNE_OK;
}

struct lsdn_node_ops lsdn_netdev_ops = {
	.free_private_data = free_netdev,
	.get_port = get_netdev_port,
	.update_rules = lsdn_noop,
	.update_ifs = lsdn_noop,
	.update_if_rules = update_if_rules
};
