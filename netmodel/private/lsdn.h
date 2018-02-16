/** \file
 * Main LSDN structure definitions - private part. */
#pragma once

#include "../include/lsdn.h"
#include "names.h"
#include "list.h"
#include "rules.h"
#include "nl.h"
#include "idalloc.h"
#include "sbridge.h"
#include "lbridge.h"
#include "state.h"

/** LSDN Context.
 * This is the central structure that keeps track of the in-memory network model as a whole.
 * Its main purpose is to allow commit, teardown and deallocation of the model in one go.
 * It also allows configuring common options (OOM callback, problem callback) and generating unique
 * object names.
 *
 * The same structures (lsdn_phys, lsdn_virt) are used to describe both remote objects
 * and objects running on other machines. This allows the orchestrator to make the same API calls
 * on all physical machines to construct the network topology. The only difference between the
 * API calls on the physical machines will be the lsdn_phys_claim_local calls.
 *
 * Typically, the network model is represented by one context. Although it is possible to 
 * keep multiple contexts in a single program, this is not recommended. Uniqueness of objects
 * (such as interface names, "one phys per physical interface" etc.) is only checked inside of
 * a single context, so multiple contexts could run into collisions. Multiple networks can exist
 * in a single context, so in practice you probably don't need more than one context anyway. */
struct lsdn_context {
	/** Context name. Determines the prefix for objects created in the context. */
	char* name;
	/** Out-of-memory callback. */
	lsdn_nomem_cb nomem_cb;
	/** User data for the OOM calback. */
	void *nomem_cb_user;

	/** Unique phys names. */
	struct lsdn_names phys_names;
	/** Unique network names. */
	struct lsdn_names net_names;
	/** Unique settings names. */
	struct lsdn_names setting_names;

	/** List of networks. */
	struct lsdn_list_entry networks_list;
	/** List of settings. */
	struct lsdn_list_entry settings_list;
	/** List of list of physes. */
	struct lsdn_list_entry phys_list;

	/** Netlink socket for installing tc rules. */
	struct mnl_socket *nlsock;

	/** User-specified problem callback. */
	lsdn_problem_cb problem_cb;
	/** User-specified data for problem callback. */
	void *problem_cb_user;

	/** @name Error handling. Only used during validation and commit phase. */
	/** @{ */
	/** Currently processed problem. */
	struct lsdn_problem problem;
	/** Problem refs for currently processed problem. */
	struct lsdn_problem_ref problem_refs[LSDN_MAX_PROBLEM_REFS];
	/** Problem count for the current operation. */
	size_t problem_count;
	bool inconsistent;
	/** @} */

	/** Disable decommit.
	 * Checked when tearing down the in-memory model. If `disable_decommit`
	 * is set, tc rules are retained in kernel. Otherwise, tc rules are removed
	 * so the networks stop working. */
	bool disable_decommit;

	/** Number of created LSDN objects.
	 * Used to assign unique names to created objects. */
	int obj_count;

	/** Name buffer.
	 * Space for rendering unique LSDN object names. */
	char namebuf[64 + 1];
};

/** Type of network encapsulation. */
enum lsdn_nettype {
	/** VXLAN encapsulation. */
	LSDN_NET_VXLAN,
	/** VLAN encapsulation. */
	LSDN_NET_VLAN,
	/** No encapsulation. */
	LSDN_NET_DIRECT,
	/** GENEVE enacpsulation */
	LSDN_NET_GENEVE
};

/** Switch type for the virtual network. */
enum lsdn_switch {
	/** Learning switch with single tunnel shared from the phys.
	 * The network is essentially autoconfiguring in this mode. */
	LSDN_LEARNING,
	/** Learning switch with a tunnel for each connected endpoint.
	 * In this mode the connection information (IP addr) for each physical node is required. */
	LSDN_LEARNING_E2E,
	/** Static switching with a tunnel for each connected endpoint.
	 * In this mode we need the connection information + MAC addresses of all virts and where
	 * they reside.
	 *
	 * @note the endpoint is represented by a single linux interface,
	 * with the actual endpoint being selected by tc actions. */
	LSDN_STATIC_E2E

	/* LSDN_STATIC does not exists, because it does not make much sense ATM. It would have
	 * static rules for the switching at local level, but it would go out through a single
	 * interface to be switched by some sort of learning switch. May be added if it appears. */
};

/** Network settings. */
struct lsdn_settings {
	/** Commit state of the settings object. */
	enum lsdn_state state;
	/** Pending delete flag. */
	bool pending_free;

	/** Membership in list of all settings.
	 * @see #lsdn_context.settings_list */
	struct lsdn_list_entry settings_entry;
	/** List of users of this settings object. */
	struct lsdn_list_entry setting_users_list;

	/** LSDN context. */
	struct lsdn_context *ctx;
	/** Associated network operations. */
	struct lsdn_net_ops *ops;
	/** Name of the settings object. */
	struct lsdn_name name;

	/** Network type. */
	enum lsdn_nettype nettype;
	/** Switch type. */
	enum lsdn_switch switch_type;

	union {
		/** Properties for the VXLAN network type. */
		struct {
			/** VXLAN UDP port. */
			uint16_t port;
			union {
				/** Properties for multicast VXLAN. */
				struct {
					/** Multicast IP address. */
					lsdn_ip_t mcast_ip;
				} mcast;
				/** Properties of end-to-end static VXLAN. */
				struct {
					/** Reference count for the tunnel interface. */
					size_t refcount;
					/** Tunnel interface. */
					struct lsdn_if tunnel;
					/** Bridge interface XXX */
					struct lsdn_sbridge_phys_if tunnel_sbridge;
					/** Ruleset XXX */
					struct lsdn_ruleset ruleset_in;
				} e2e_static;
			};
		} vxlan;
		/** Properties for the GENEVE network type. */
		struct {
			/** GENEVE UDP port. */
			uint16_t port;
			size_t refcount;
			/** Tunnel interface. */
			struct lsdn_if tunnel;
			/** Bridge interface XXX */
			struct lsdn_sbridge_phys_if tunnel_sbridge;
			/** Ruleset XXX */
			struct lsdn_ruleset ruleset_in;
		} geneve;
	};

	/** User callback hooks. */
	struct lsdn_user_hooks *user_hooks;
};

/** LSDN phys.
 * Phys is a representation of a physical machine hosting tenants of the LSDN network.
 * Virts (tenant representations) can be attached to a phys, detached, and migrated
 * between different physes, to reflect that a virtual machine is being moved to
 * a different hardware. This is transparent to the virtual network, apart from
 * disconnect and reconnect events.
 *
 * Exactly one phys should be marked as local, to indicate that it represents the
 * point of view of the currently running LSDN instance. Routing rules for interfaces
 * on the local phys will be commited to the kernel. */
struct lsdn_phys {
	/** Commit state of the phys object. */
	enum lsdn_state state;
	/** Pending delete flag. */
	bool pending_free;

	/** Phys name. */
	struct lsdn_name name;
	/** Membership in list of all physes.
	 * @see #lsdn_context.phys_list. */
	struct lsdn_list_entry phys_entry;
	/** List of attached virts. */
	struct lsdn_list_entry attached_to_list;

	/** LSDN context. */
	struct lsdn_context* ctx;
	/** Local flag.
	 * If set to true, this phys object represents the point of view of the running
	 * LSDN instance. */
	bool is_local;
	/** Local committed flag.
	 * Indicated that routing rules are installed in the kernel as if this is the local phys. */
	bool committed_as_local;
	/** Outgoing network interface name.
	 * Represents the interface that is connected to the physical network. Other physes
	 * should be reachable through this interface. */
	char *attr_iface;
	/** IP address on the physical network. */
	lsdn_ip_t *attr_ip;
};

struct lsdn_net {
	enum lsdn_state state;
	bool pending_free;
	struct lsdn_list_entry networks_entry;
	struct lsdn_list_entry settings_users_entry;
	struct lsdn_context *ctx;
	struct lsdn_settings *settings;
	struct lsdn_name name;

	uint32_t vnet_id;
	struct lsdn_list_entry virt_list;
	/* List of lsdn_phys_attachement attached to this network */
	struct lsdn_list_entry attached_list;
	struct lsdn_names virt_names;
};

struct lsdn_phys_attachment {
	enum lsdn_state state;
	bool pending_free;
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
	 * explicitly attached, we assume you just made a mistake.
	 */
	bool explicitly_attached;

	struct lsdn_if tunnel_if;
	struct lsdn_lbridge lbridge;
	struct lsdn_lbridge_if lbridge_if;

	struct lsdn_sbridge sbridge;
	struct lsdn_sbridge_if sbridge_if;
};

struct lsdn_virt {
	/* Tracks the state of local virts */
	enum lsdn_state state;
	bool pending_free;
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
	lsdn_qos_rate_t *attr_rate_in;
	lsdn_qos_rate_t *attr_rate_out;
	/*lsdn_ip_t *attr_ip; */

	union {
		struct {
			struct lsdn_lbridge_if lbridge_if;
		};

		struct {
			struct lsdn_sbridge_if sbridge_if;
			struct lsdn_sbridge_phys_if sbridge_phys_if;
			struct lsdn_sbridge_route sbridge_route;
			struct lsdn_sbridge_mac sbridge_mac;
		};
	};

	/** A rule for doing the policing on virt's ingress (our egress) */
	struct lsdn_ruleset_prio *commited_policing_in;
	struct lsdn_rule commited_policing_rule_in;
	/** A rule for doing the policing on virt's egress (our ingress) */
	struct lsdn_ruleset_prio *commited_policing_out;
	struct lsdn_rule commited_policing_rule_out;

	/** A ruleset on virt's egress (our ingress) */
	struct lsdn_ruleset rules_in;
	/** A ruleset on virt's ingress (our egress) */
	struct lsdn_ruleset rules_out;
	struct vr_prio *ht_in_rules;
	struct vr_prio *ht_out_rules;
};
