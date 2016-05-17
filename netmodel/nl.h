#ifndef _LSDN_NL_H
#define _LSDN_NL_H

#include "errors.h"

#include <libmnl/libmnl.h>

#include <linux/if_link.h>
#include <linux/rtnetlink.h>

lsdn_err_t lsdn_socket_init(struct mnl_socket **sock);

lsdn_err_t lsdn_link_dummy_create(struct mnl_socket *sock, const char *ifname);

#endif
