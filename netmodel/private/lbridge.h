#ifndef LSDN_LBRIDGE_H
#define LSDN_LBRIDGE_H

#include "net.h"

/* Following functions are helpers for the linux bridge network types */
/** Create a bridge interface for the attachment and connect all virts */
void lsdn_lbridge_make(struct lsdn_phys_attachment *phys);
/** Connect all tunnels to the bridge created by lsdn_net_make_bridge */
void lsdn_lbridge_connect(struct lsdn_phys_attachment *phys);

#endif
