#ifndef _LSDN_NL_H
#define _LSDN_NL_H

#include "errors.h"

#include <time.h>
#include <string.h>

#include <libmnl/libmnl.h>

#include <net/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>

struct mnl_socket *lsdn_socket_init();

int lsdn_link_dummy_create(struct mnl_socket *sock, const char *if_name);

int lsdn_link_set(struct mnl_socket *sock, const char *if_name, bool up);

int lsdn_qdisc_htb_create(struct mnl_socket *sock, unsigned int if_index,
		uint32_t parent, uint32_t handle, uint32_t r2q, uint32_t defcls);

#endif
