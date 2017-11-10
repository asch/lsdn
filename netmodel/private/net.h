#pragma once

#include "../include/lsdn.h"

const char *lsdn_mk_ifname(struct lsdn_context* ctx);
void lsdn_settings_init_common(struct lsdn_settings *settings, struct lsdn_context *ctx);

/*
 * A per-local PA view of a remote PA. This structure exists for each combination
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

/*
 * A per-local PA view of a remote virt. This structure exists for each combination
 * of (local PA, virt on any other PA).
 */
struct lsdn_remote_virt {
	struct lsdn_list_entry virt_view_entry;
	struct lsdn_list_entry remote_virt_entry;
	struct lsdn_remote_pa *pa;
	struct lsdn_virt *virt;

	struct lsdn_sbridge_mac sbridge_mac;
};

/* Calbacks implemented by concrete network types. The documentation at each callback should be
 * taken as a hint as to what actions you might want to take, in order to make the network work.
 */
struct lsdn_net_ops {
	/* This machine has connected to a network, create:
	 *
	 * - bridge (lbridge or sbridge)
	 * - tunnel interface (interface + lbridge if or interface + sbridge_phys_if + sbridge_if)
	 * - or connect an existing shared tunnel (sbridge_if)
	 */
	void (*create_pa) (struct lsdn_phys_attachment *pa);
	/* A virt has connected to a network, create:
	 *
	 * - bridge interface (lbridge_if or sbridge_phys_if + sbridge_if)
	 */
	void (*add_virt) (struct lsdn_virt *virt);
	/* Another machine has connected to this network. Create:
	 *
	 * - a routing rule (sbridge_route)
	 * - or nothing, if the network does not need routing information
	 */
	void (*add_remote_pa) (struct lsdn_remote_pa *pa);
	/* A virt has connected on a remote machine. `add_remote_pa was` already
	 * called for the remote PA. Create:
	 *
	 * - a MAC address match on a routing rule (sbridge_mac)
	 * - or nothing, if the network does not need routing information
	 */
	void (*add_remote_virt) (struct lsdn_remote_virt *virt);

	/* Destroy all resources created by create_pa. All virts, remote virts and remote PAs were
	 * already removed and this PA is empty */
	void (*destroy_pa) (struct lsdn_phys_attachment *pa);
	/* Clean up after a local virt. */
	void (*remove_virt) (struct lsdn_virt *virt);
	/* Clean up after a remote PA. All its remote virts were already removed. */
	void (*remove_remote_pa) (struct lsdn_remote_pa *pa);
	/* Clean up after a remote virt. */
	void (*remove_remote_virt) (struct lsdn_remote_virt *virt);

	void (*validate_pa) (struct lsdn_phys_attachment *pa);
	void (*validate_virt) (struct lsdn_virt *virt);
};
