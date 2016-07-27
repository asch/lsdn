#ifndef _LINUX_SWITCH_H_
#define _LINUX_SWITCH_H_

#include <stddef.h>
#include "network.h"
#include "node.h"

LSDN_DEFINE_NODE(linux_switch)

struct lsdn_linux_switch *lsdn_linux_switch_new(
		struct lsdn_network *net,
		size_t port_count);

#endif
