#ifndef _LSDN_H
#define _LSDN_H

#include <stdint.h>
#include <stdbool.h>
#include "../private/list.h"
#include "../private/rule.h"
#include "nettypes.h"

/**
 * A top-level object encompassing all network topology. This includes virtual networks
 * (lsdn_network) and physical host connections (lsdn_phys). Only one context will typically exist
 * in a given program.
 *
 * The same structures (lsdn_phys, lsdn_virt) are used to describe both remote objects
 * and objects running on other machines. This allows the orchestrator to make the same API calls
 * on all physical machines to construct the network topology. The only difference between the
 * API calls on the physical machines will be the lsdn_phys_claim_local calls.
 */
struct lsdn_context{
	/* Determines the prefix for interfaces created in the context */
	char* name;

	struct lsdn_list_entry networks_list;

	struct lsdn_list_entry phys_list;
};

enum lsdn_nettype{LSDN_NT_VXLAN, LSDN_NT_VLAN, LSDN_NT_DIRECT};
enum lsdn_switch{LSDN_LEARNING, LSDN_STATIC};

/**
 * Virtual network to which nodes (lsdn_virt) connect through physical host connections (lsdn_phys).
 * Can be implemented using common tunneling techniques, like vlan or vxlan or no tunneling.
 *
 * Networks are defined by two main characteristics:
 *  - the tunnel used to overlay the network over physical topology (transparent to end users)
 *  - the switching methods used (visible to end users)
 */
struct lsdn_network {
	struct lsdn_list_entry networks_entry;

	struct lsdn_context* ctx;
	char* name;
	struct lsdn_list_entry virt_list;
	/* List of lsdn_phys_attachement attached to this network */
	struct lsdn_list_entry attached_list;
	enum lsdn_nettype nettype;
	union {
		uint32_t vlan_id;
		uint32_t vxlan_id;
	};

	enum lsdn_switch switch_type;
};

/**
 * Represents a physical host connection (e.g. eth0 on lsdn1).
 * Physical interfaces are used to connect to virtual networks. This connection is called
 * lsdn_phys_attachement.
 */
struct lsdn_phys {
	struct lsdn_list_entry phys_entry;
	struct lsdn_list_entry attached_to_list;

	struct lsdn_context* ctx;
	char *attr_iface;
	lsdn_ip_t *attr_ip;
};

/**
 * A point of connection to a virtual network through a physical interface.
 * Only single attachment may exist for a pair of a physical connection and network.
 */
struct lsdn_phys_atttachment {
	struct lsdn_list_entry attached_entry;
	struct lsdn_list_entry attached_to_entry;
	struct lsdn_list_entry connected_virt_list;

	struct lsdn_network net;
	struct lsdn_phys phys;

	/* we will probably have tunnel info/bridge info here */
};

/**
 * A virtual machine (typically -- it may be any linux interface).
 *
 * Virtual machines participate in virtual networks (through phys_attachments on the host machine
 * connection). They can be migrated between the physical machines by connecting them through
 * different lsdn_phys.
 */
struct lsdn_virt {
	struct lsdn_list_entry virt_entry;
	struct lsdn_list_entry connected_virt_entry;
	struct lsdn_network* network;

	struct lsdn_list_entry virt_rules_list;

	struct lsdn_phys* connected_through;
	char* connected_iface;

	lsdn_mac_t *attr_mac;
	/*lsdn_ip_t *attr_ip; */
};

/**
 * An entry in routing/forwarding table for a given virt. This may serve as a template for multiple
 * rules in different ruleset instances.
 */
struct lsdn_virt_rule{
	/* list in lsdn_virt */
	struct lsdn_list_entry virt_rules_entry;
	/* entries in lsdn_rule */
	struct lsdn_list_entry rule_instance_list;

	struct lsdn_virt *owning_virt;
	struct lsdn_match match;
};

#endif
