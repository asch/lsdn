/** \file
 * Common structs and definitions for various kinds of networks. */
#pragma once

#include "../include/lsdn.h"
#include "lsdn.h"

const char *lsdn_mk_ifname(struct lsdn_context* ctx);
lsdn_err_t lsdn_prepare_rulesets(
	struct lsdn_context *ctx, struct lsdn_if *iface,
	struct lsdn_ruleset* in, struct lsdn_ruleset* out);
void lsdn_settings_init_common(struct lsdn_settings *settings, struct lsdn_context *ctx);

/** Per-local PA view of a remote PA. TODO
 * This structure exists for each combination
 * of (local PA, any other PA).
 */
struct lsdn_remote_pa {
	struct lsdn_list_entry pa_view_entry;
	struct lsdn_list_entry remote_pa_entry;
	struct lsdn_list_entry remote_virt_list;
	struct lsdn_phys_attachment *local;
	struct lsdn_phys_attachment *remote;

	struct lsdn_sbridge_route sbridge_route;
};

/** Per-local PA view of a remote virt. TODO
 * This structure exists for each combination
 * of (local PA, virt on any other PA).
 */
struct lsdn_remote_virt {
	struct lsdn_list_entry virt_view_entry;
	struct lsdn_list_entry remote_virt_entry;
	struct lsdn_remote_pa *pa;
	struct lsdn_virt *virt;

	struct lsdn_sbridge_mac sbridge_mac;
};

/** Implementations of network operations.
 * Callbacks implemented by a particular network type.
 * The documentation at each callback should be taken as a hint as to what actions
 * you might want to take, in order to make the network work. */
struct lsdn_net_ops {
	/** Create a local phys attachment.
	 * Called when the local machine connects to a network.
	 *
	 * You can create:
	 * - bridge (lbridge or sbridge)
	 * - tunnel interface (interface + lbridge if or interface + sbridge_phys_if + sbridge_if)
	 * - or connect an existing shared tunnel (sbridge_if)
	 */
	void (*create_pa) (struct lsdn_phys_attachment *pa);

	/** Add a local virtual machine.
	 * Called when a virt on the local machine is added to a network.
	 *
	 * We use this to create a bridge interface (lbridge_if or sbridge_phys_if + sbridge_if)
	 */
	void (*add_virt) (struct lsdn_virt *virt);

	/** Add a remote machine.
	 * Called when a remote machine connects to our network. Create:
	 *
	 * - a routing rule (sbridge_route)
	 * - or nothing, if the network does not need routing information
	 */
	void (*add_remote_pa) (struct lsdn_remote_pa *pa);

	/** Add a remote virtual machine.
	 * Called when a virt on a remoe machine connects to our network.
	 * You can assume that the remote machine was already added
	 * through `add_remote_pa`. Create:
	 *
	 * - a MAC address match on a routing rule (sbridge_mac)
	 * - or nothing, if the network does not need routing information
	 */
	void (*add_remote_virt) (struct lsdn_remote_virt *virt);

	/** Clean up after the local machine.
	 * Called when local machine is disconnecting from the network.
	 * Destroy all resources created by `create_pa`.
	 * All virts, remote virts and remote PAs were already removed and this PA is empty */
	void (*destroy_pa) (struct lsdn_phys_attachment *pa);

	/** Clean up after a local virt. */
	void (*remove_virt) (struct lsdn_virt *virt);

	/** Clean up after a remote machine.
	 * All its remote virts were already removed. */
	void (*remove_remote_pa) (struct lsdn_remote_pa *pa);

	/** Clean up after a remote virt. */
	void (*remove_remote_virt) (struct lsdn_remote_virt *virt);

	/** Validate a machine.
	 * Called when adding local or remote machine.
	 * You can validate attributes relevant to the network implementation
	 * and use `lsdn_problem_report` to indicate problems. */
	void (*validate_pa) (struct lsdn_phys_attachment *pa);

	/** Validate a virt.
	 * Called when adding local or remote virt.
	 * You can validate attributes relevant to the network implementation
	 * and use `lsdn_problem_report` to indicate problems. */
	void (*validate_virt) (struct lsdn_virt *virt);

	/** Get the recommended MTU.
	 * Get the recommended MTU for a virt on a given phys attachment. */
	unsigned int (*get_recommended_mtu) (struct lsdn_phys_attachment *pa);
};
