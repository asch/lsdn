#ifndef _LSDN_STATIC_SWITCH_H
#define _LSDN_STATIC_SWITCH_H

#include "node.h"
#include "nettypes.h"
#include <stddef.h>

LSDN_DEFINE_NODE(static_switch)

struct lsdn_static_switch *lsdn_static_switch_new(
		struct lsdn_network *net,
		size_t port_count);

lsdn_err_t lsdn_static_switch_add_rule(
		struct lsdn_static_switch *sswitch,
		const lsdn_mac_t *dst_mac,
		size_t port);

#endif
