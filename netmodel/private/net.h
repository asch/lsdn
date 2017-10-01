#ifndef _LSDN_NET_H
#define _LSDN_NET_H

#include "../include/lsdn.h"

const char *lsdn_mk_ifname(struct lsdn_context* ctx);

struct lsdn_net_ops {
	/* Setup the connection between the virts on the same network and create a tunnel
	 * out to the virts on other physical machines */
	void (*create_pa) (struct lsdn_phys_attachment *phys);
	void (*validate_pa) (struct lsdn_phys_attachment *pa);
	void (*validate_virt) (struct lsdn_virt *virt);
};

/** Activate the linux bridge and tunnel interfaces */
void lsdn_net_set_up(struct lsdn_phys_attachment *a);

#endif
