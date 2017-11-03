#ifndef _LSDN_NET_H
#define _LSDN_NET_H

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

struct lsdn_net_ops {
	/* Create a local physical attachment to a network and needed related interfaces, such as
	 * bridges or tunnels (if there is one tunnel-per local attachment). */
	void (*create_pa) (struct lsdn_phys_attachment *pa);
	/* A virt has conncted on this machine. A per-PA tunnel might be created here. */
	void (*add_virt) (struct lsdn_virt *virt);
	/* Another machine has connected to this network. A routing rule
	 * might be created here and virt might be connected to a bridge. */
	void (*add_remote_pa) (struct lsdn_remote_pa *pa);
	/* A virt has connected on a remote PA. add_remote_pa was already
	 * called for the remote PA. A routing rule might be created here. */
	void (*add_remote_virt) (struct lsdn_remote_virt *virt);

	/* Destroy all resources created by create_pa. All virts, remote virts and remote PAs were
	 * already removed and this PA is empty */
	void (*destroy_pa) (struct lsdn_phys_attachment *pa);
	/* Clean up after a local virt. */
	void (*remove_virt) (struct lsdn_virt *virt);
	/* Clean up after a remote PA. All its remote nodes were already removed. */
	void (*remove_remote_pa) (struct lsdn_remote_pa *pa);
	/* Clean up after a remote virt. */
	void (*remove_remote_virt) (struct lsdn_remote_virt *virt);

	void (*validate_pa) (struct lsdn_phys_attachment *pa);
	void (*validate_virt) (struct lsdn_virt *virt);
};

#endif
