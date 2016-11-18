#include <net/if.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>
#include "include/linux_switch.h"
#include "private/node.h"
#include "private/port.h"
#include "private/network.h"
#include "private/rule.h"
#include "include/util.h"
#include "private/nl.h"

static struct lsdn_port_ops port_ops;

struct switch_port {
	struct lsdn_port port;
	struct lsdn_if bridged_if;
	/* This veth pair is used in cases where mirred to ingress
	 * is not supported. */
	struct lsdn_if veth_if;

	/* The port's ruleset just redirects to bridged_if ingress
	 * or veth_if egress */
	struct lsdn_ruleset ruleset;
	struct lsdn_rule rule;
};

struct lsdn_linux_switch {
	struct lsdn_node node;
	struct lsdn_if bridge_if;;
};

struct lsdn_linux_switch *lsdn_linux_switch_new(
		struct lsdn_network *net)
{
	struct lsdn_linux_switch *lswitch = (struct lsdn_linux_switch *)
			lsdn_node_new(net, &lsdn_linux_switch_ops, sizeof(*lswitch));
	lsdn_init_if(&lswitch->bridge_if);
	lsdn_commit_to_network(&lswitch->node);
	return lswitch;
}

static struct lsdn_port* new_port(struct lsdn_node *node, lsdn_port_type_t type)
{
	lsdn_as_linux_switch(node);
	assert(type == LSDN_PORTT_DEFAULT);

	struct switch_port* port = malloc(sizeof(struct switch_port));
	if(!port)
		return NULL;
	port->port.ops = &port_ops;
	lsdn_ruleset_init(&port->ruleset);
	lsdn_rule_init(&port->rule);
	port->rule.action.id = LSDN_ACTION_IF;
	/* the interface will be filled in later when we actually create it */
	lsdn_add_rule(&port->ruleset, &port->rule, 0);
	lsdn_init_if(&port->bridged_if);
	lsdn_init_if(&port->veth_if);
	port->port.ruleset = &port->ruleset;

	return &port->port;
}

static void free_port(struct lsdn_port *port_gen){
	struct switch_port* port = (struct switch_port*)port_gen;
	lsdn_ruleset_free(&port->ruleset);
	lsdn_destroy_if(&port->veth_if);
	lsdn_destroy_if(&port->bridged_if);

	/* free all rules */
	struct lsdn_rule *prev = lsdn_container_of(
		port->ruleset.rules.next,
		struct lsdn_rule,
		ruleset_list);
	struct lsdn_rule *cur = prev;
	while (!lsdn_is_list_empty(&port->ruleset.rules)) {
		cur = lsdn_container_of(
				cur->ruleset_list.next,
				struct lsdn_rule,
				ruleset_list);
		lsdn_rule_free(prev);
		prev = cur;
	}
}

static void free_lswitch(struct lsdn_node *node)
{
	struct lsdn_linux_switch* lswitch = lsdn_as_linux_switch(node);

	lsdn_destroy_if(&lswitch->bridge_if);
}

static lsdn_err_t lswitch_update_rules(struct lsdn_node *node)
{
	lsdn_foreach_list(node->ports, ports, struct lsdn_port, port_gen){
		struct switch_port* p = (struct switch_port*) port_gen;
		p->rule.action.ifname = p->veth_if.ifname;
	}

	return LSDNE_OK;
}

static lsdn_err_t lswitch_update_ifs(struct lsdn_node *node)
{
	struct mnl_socket* sock = lsdn_socket_init();
	struct lsdn_linux_switch* lswitch = lsdn_as_linux_switch(node);
	lsdn_err_t ret = LSDNE_OK;
	int err;

	char brname[IF_NAMESIZE];
	snprintf(brname, IF_NAMESIZE, "%s-%d", node->network->name, ++node->network->unique_id);
	err = lsdn_link_bridge_create(sock, &lswitch->bridge_if, brname);
	if(err != 0){
		ret = LSDNE_FAIL;
		goto err;
	}

	char ifname1[IF_NAMESIZE];
	char ifname2[IF_NAMESIZE];
	lsdn_foreach_list(node->ports, ports, struct lsdn_port, port_gen){
		struct switch_port *port = (struct switch_port*) port_gen;
		int id = ++node->network->unique_id;
		snprintf(ifname1, IF_NAMESIZE, "%s-%da", node->network->name, id);
		snprintf(ifname2, IF_NAMESIZE, "%s-%db", node->network->name, id);
		err = lsdn_link_veth_create(sock, &port->bridged_if, ifname1, &port->veth_if, ifname2);
		if(err != 0){
			ret = LSDNE_FAIL;
			break;
		}

		err = lsdn_link_set(sock, port->bridged_if.ifindex, 1);
		if(err != 0){
			ret = LSDNE_FAIL;
			break;
		}

		err = lsdn_link_set(sock, port->veth_if.ifindex, 1);
		if(err != 0){
			ret = LSDNE_FAIL;
			break;
		}

		err = lsdn_link_set_master(sock,
					lswitch->bridge_if.ifindex, port->bridged_if.ifindex);
		if(err != 0){
			ret = LSDNE_FAIL;
			break;
		}
	}

	err = lsdn_link_set(sock, lswitch->bridge_if.ifindex, 1);
	if(err != 0)
		ret = LSDNE_FAIL;

	err:
	lsdn_socket_free(sock);

	return ret;
}

static lsdn_err_t lswitch_update_if_rules(struct lsdn_node *node)
{
	lsdn_as_linux_switch(node);

	struct mnl_socket* sock = lsdn_socket_init();
	lsdn_err_t ret = LSDNE_OK;

	lsdn_foreach_list(node->ports, ports, struct lsdn_port, port_gen){
		struct switch_port *port = (struct switch_port*) port_gen;
		if (!port->port.peer)
			continue;

		int err = lsdn_qdisc_htb_create(sock, port->bridged_if.ifindex,
				TC_H_ROOT, LSDN_DEFAULT_EGRESS_HANDLE, 10, 0);
		if (err != 0){
			ret = LSDNE_FAIL;
			break;
		}

		struct lsdn_filter *filter = lsdn_filter_init(
					"flower", port->bridged_if.ifindex,
					1,
					LSDN_DEFAULT_EGRESS_HANDLE,
					LSDN_DEFAULT_PRIORITY,
					ETH_P_ALL<<8 /* magic trick 1 */);

		lsdn_filter_actions_start(filter, TCA_FLOWER_ACT);

		struct lsdn_if* dst_if = &port->port.peer->ruleset->interface;
		assert(lsdn_if_created(dst_if));
		unsigned int ifindex_dst = dst_if->ifindex;

		lsdn_action_mirred_add(filter, 1, TC_ACT_STOLEN, TCA_EGRESS_REDIR,
				       ifindex_dst);

		lsdn_filter_actions_end(filter);

		err = lsdn_filter_create(sock, filter);
		printf("Created linux-brigde rules on %s (%d)\n",
		       port->bridged_if.ifname, port->bridged_if.ifindex);
		if (err != 0){
			ret = LSDNE_FAIL;
			break;
		}

		lsdn_filter_free(filter);
	}

	lsdn_socket_free(sock);
	return ret;
}

static struct lsdn_port_ops port_ops = {
	.free = free_port
};

struct lsdn_node_ops lsdn_linux_switch_ops = {
	.free_private_data = free_lswitch,
	.new_port =  new_port,
	/* Lswitch rules are set-up at port creation time. */
	.update_rules = lswitch_update_rules,
	.update_ifs = lswitch_update_ifs,
	.update_if_rules = lswitch_update_if_rules
};
