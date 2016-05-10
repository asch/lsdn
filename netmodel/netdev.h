#ifndef _LSDN_NETDEV_H
#define _LSDN_NETDEV_H

#include "node.h"

LSDN_DEFINE_NODE(netdev)

/**
 * @brief Creates a new logical network node representing an existing linux
 *        interface.
 *
 * The netdev node has only a single port, representing that interface.
 */
struct lsdn_netdev *lsdn_netdev_new(
		struct lsdn_network *net,
		const char *linux_if);

#endif
