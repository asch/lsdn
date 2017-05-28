#ifndef _LSDN_NET_H
#define _LSDN_NET_H

struct lsdn_phys_attachment;
struct lsdn_context;
const char *lsdn_mk_ifname(struct lsdn_context* ctx);

struct lsdn_net_ops {
	/* Create a broadcast tunnel for learning switch */
	void (*mktun_br) (struct lsdn_phys_attachment *phys);
};

#endif
