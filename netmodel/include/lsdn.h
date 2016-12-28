#ifndef _LSDN_H
#define _LSDN_H

#include <stdint.h>
#include <stdbool.h>
#include "../private/list.h"
#include "nettypes.h"

struct lsdn_context{
	/* Determines the prefix for interfaces created in the context */
	char* name;

	struct lsdn_list_entry networks_list;

	struct lsdn_list_entry phys_list;
};

enum lsdn_nettype{LSDN_NT_VXLAN, LSDN_NT_VLAN};
enum lsdn_switch{LSDN_LEARNING, LSDN_STATIC};

struct lsdn_network {
	struct lsdn_list_entry networks_entry;

	struct lsdn_context* ctx;
	char* name;
	struct lsdn_list_entry virt_list;
	/* List of lsdn_phys_attachement attached to this network */
	struct lsdn_list_entry attached_list;
	enum lsdn_nettype nettype;
	union{
		uint32_t vlan_id;
		uint32_t vxlan_id;
	};

	enum lsdn_switch switch_type;
};

struct lsdn_phys {
	struct lsdn_list_entry phys_entry;
	struct lsdn_list_entry attached_to_list;

	struct lsdn_context* ctx;
	char *attr_iface;
	lsdn_ip_t *attr_ip;
};

struct lsdn_phys_atttachment {
	struct lsdn_list_entry attached_entry;
	struct lsdn_list_entry attached_to_entry;

	struct lsdn_network net;
	struct lsdn_phys phys;

	/* we will probably have tunnel info/bridge info here */
};

struct lsdn_virt{
	struct lsdn_list_entry virt_entry;
	struct lsdn_network* network;
	/* list of lsdn_rules which are present to implement forwarding to this virt*/
	struct lsdn_list_entry virt_rules_list;

	struct lsdn_phys* connected_through;
	char* connected_iface;

	lsdn_mac_t *attr_mac;
	/*lsdn_ip_t *attr_ip; */
};

#endif
