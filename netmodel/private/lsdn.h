#pragma once

#include "../include/lsdn.h"
#include "names.h"
#include "list.h"
#include "rules.h"
#include "nl.h"
#include "idalloc.h"
#include "sbridge.h"
#include "lbridge.h"


/** State of the LSDN object. */
enum lsdn_state {
	/** Object is being committed for a first time. */
	LSDN_STATE_NEW,
	/** Object was already committed and needs to be recommitted. */
	LSDN_STATE_RENEW,
	/** Object is already committed and needs to be deleted. */
	LSDN_STATE_DELETE,
	/** Object is in committed state. */
	LSDN_STATE_OK
};

struct lsdn_context{
	/** Context name. Determines the prefix for interfaces created in the context. */
	char* name;
	/** Out-of-memory callback. */
	lsdn_nomem_cb nomem_cb;
	/** User data for the OOM calback. */
	void *nomem_cb_user;
	struct lsdn_names phys_names;
	struct lsdn_names net_names;
	struct lsdn_names setting_names;

	struct lsdn_list_entry networks_list;
	struct lsdn_list_entry settings_list;
	struct lsdn_list_entry phys_list;
	struct mnl_socket *nlsock;

	// error handling -- only valid during validation and commit
	struct lsdn_problem problem;
	struct lsdn_problem_ref problem_refs[LSDN_MAX_PROBLEM_REFS];
	lsdn_problem_cb problem_cb;
	void *problem_cb_user;
	size_t problem_count;
	bool disable_decommit;

	int ifcount;
	char namebuf[IF_NAMESIZE + 1];
};

struct lsdn_settings {
	enum lsdn_state state;
	struct lsdn_list_entry settings_entry;
	struct lsdn_list_entry setting_users_list;
	struct lsdn_context *ctx;
	struct lsdn_net_ops *ops;
	struct lsdn_name name;

	enum lsdn_nettype nettype;
	enum lsdn_switch switch_type;
	union {
		struct {
			uint16_t port;
			union{
				struct mcast {
					lsdn_ip_t mcast_ip;
				} mcast;
				struct {
					size_t refcount;
					struct lsdn_if tunnel;
					struct lsdn_sbridge_phys_if tunnel_sbridge;
				} e2e_static;
			};
		} vxlan;
	};

	struct lsdn_user_hooks *user_hooks;
};

struct lsdn_phys {
	enum lsdn_state state;
	struct lsdn_name name;
	struct lsdn_list_entry phys_entry;
	struct lsdn_list_entry attached_to_list;

	struct lsdn_context* ctx;
	bool is_local;
	bool committed_as_local;
	char *attr_iface;
	lsdn_ip_t *attr_ip;
};

struct lsdn_net {
	enum lsdn_state state;
	struct lsdn_list_entry networks_entry;
	struct lsdn_list_entry settings_users_entry;
	struct lsdn_context *ctx;
	struct lsdn_settings *settings;
	struct lsdn_name name;

	enum lsdn_ipv ipv;
	uint32_t vnet_id;
	struct lsdn_list_entry virt_list;
	/* List of lsdn_phys_attachement attached to this network */
	struct lsdn_list_entry attached_list;
	struct lsdn_names virt_names;
};

struct lsdn_phys_attachment {
	enum lsdn_state state;
	/* list held by net */
	struct lsdn_list_entry attached_entry;
	/* list held by phys */
	struct lsdn_list_entry attached_to_entry;
	struct lsdn_list_entry connected_virt_list;
	/* List of remote PAs that correspond to this PA */
	struct lsdn_list_entry pa_view_list;
	/* List of remote PAs that this PA can see in the network */
	struct lsdn_list_entry remote_pa_list;

	struct lsdn_net *net;
	struct lsdn_phys *phys;
	/** Was this attachment created by lsdn_phys_attach at some point, or was it implicitely
	 * created by lsdn_virt_connect, just for bookkeeping? All fields below are valid only for
	 * explicit attachments. If you try to commit a network and some attachment is not
	 * explicitely attached, we assume you just made a mistake.
	 */
	bool explicitely_attached;

	struct lsdn_if tunnel_if;
	struct lsdn_lbridge lbridge;
	struct lsdn_lbridge_if lbridge_if;

	struct lsdn_sbridge sbridge;
	struct lsdn_sbridge_if sbridge_if;
};

struct lsdn_virt {
	/* Tracks the state of local virts */
	enum lsdn_state state;
	struct lsdn_name name;
	struct lsdn_list_entry virt_entry;
	struct lsdn_list_entry connected_virt_entry;
	struct lsdn_list_entry virt_view_list;
	struct lsdn_net* network;

	struct lsdn_phys_attachment* connected_through;
	struct lsdn_phys_attachment* committed_to;
	struct lsdn_if connected_if;
	struct lsdn_if committed_if;

	lsdn_mac_t *attr_mac;
	/*lsdn_ip_t *attr_ip; */

	struct lsdn_lbridge_if lbridge_if;

	struct lsdn_sbridge_if sbridge_if;
	struct lsdn_sbridge_phys_if sbridge_phys_if;
	struct lsdn_sbridge_route sbridge_route;
	struct lsdn_sbridge_mac sbridge_mac;
};
