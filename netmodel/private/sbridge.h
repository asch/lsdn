#ifndef LSDN_SBRIDGE_H
#define LSDN_SBRIDGE_H

#include "../include/lsdn.h"

/* Following functions are helpers for the static flower-based network types */
/**
 * Setup the attachment's dummy interface and fill in/update its switching and broadcast rules
 * for local virts.
 */
void lsdn_sbridge_setup(
	struct lsdn_phys_attachment *a);
void lsdn_sbridge_init_shared_tunnel(struct lsdn_context *ctx, struct lsdn_shared_tunnel* tun);
void lsdn_sbridge_connect_shared_tunnel(
	struct lsdn_phys_attachment *a,
	struct lsdn_shared_tunnel *tunnel);
/**
 * Activate the dummy static switching interface.
 */
void lsdn_sbridge_set_up(struct lsdn_phys_attachment* a);

#endif
