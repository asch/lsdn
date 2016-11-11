#ifndef _LSDN_STATIC_SWITCH_H
#define _LSDN_STATIC_SWITCH_H

#include "node.h"
#include "nettypes.h"
#include <stddef.h>

struct lsdn_port;

LSDN_DEFINE_NODE(static_switch)

struct lsdn_static_switch *lsdn_static_switch_new(struct lsdn_network *net);

void lsdn_static_switch_enable_broadcast(
		struct lsdn_static_switch* sswitch,
		int enabled);

lsdn_err_t lsdn_static_switch_add_rule(
		const lsdn_mac_t *dst_mac,
		struct lsdn_port *port);

#endif
