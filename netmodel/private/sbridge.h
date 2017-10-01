#ifndef LSDN_SBRIDGE_H
#define LSDN_SBRIDGE_H

#include "../include/lsdn.h"

/* Following functions are helpers for the static flower-based network types */
/**
 * Setup the attachment's dummy interface and fill in/update its switching and broadcast rules
 * for local virts.
 */
void lsdn_net_setup_static_bridge(
	struct lsdn_phys_attachment *a);
void lsdn_net_init_static_tunnel(struct lsdn_context *ctx, struct lsdn_shared_tunnel* tun);
void lsdn_net_connect_shared_static_tunnel(
	struct lsdn_phys_attachment *a,
	struct lsdn_shared_tunnel *tunnel);
/**
 * Activate the dummy static switching interface.
 */
void lsdn_net_set_static_up(struct lsdn_phys_attachment* a);

#endif
