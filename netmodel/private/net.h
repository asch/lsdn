#ifndef _LSDN_NET_H
#define _LSDN_NET_H

#include "../include/lsdn.h"

struct lsdn_phys_attachment;
struct lsdn_context;
struct lsdn_virt;
const char *lsdn_mk_ifname(struct lsdn_context* ctx);

struct lsdn_net_ops {
	/* Create a broadcast tunnel for learning switch */
	void (*mktun_br) (struct lsdn_phys_attachment *phys);
	void (*validate_pa) (struct lsdn_phys_attachment *pa);
	void (*validate_virt) (struct lsdn_virt *virt);
};

struct lsdn_net* lsdn_net_new_common(
	struct lsdn_context *ctx,
	enum lsdn_nettype type, enum lsdn_switch stype);

#endif
