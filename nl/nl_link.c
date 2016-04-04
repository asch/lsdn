#include "nl_link.h"

#include <linux/if.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/tc.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/class.h>
#include <netlink/route/classifier.h>
#include <netlink/route/addr.h>

#include <unistd.h>
#include <stdio.h>

struct rtnl_link *rtnl_link_dummy_alloc(void){
	struct rtnl_link *link;
	int err;

	if (!(link = rtnl_link_alloc()))
		return NULL;
	if ((err = rtnl_link_set_type(link, "dummy")) < 0) {
		rtnl_link_put(link);
		return NULL;
	}

	return link;
}

int nl_link_create_dummy(const char* if_name){
	struct rtnl_link *link;
	struct nl_sock *sk;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		goto error;
	}

	link = rtnl_link_dummy_alloc();
	if (link == NULL){
		err = NLE_NOMEM;
		nl_perror(err, "Unable to allocate interface");
		goto error;
	}

	rtnl_link_set_name(link, if_name);

	err = rtnl_link_add(sk, link, NLM_F_CREATE | NLM_F_EXCL);
	if (err < 0)
		nl_perror(err, "Unable to create interface");

	rtnl_link_put(link);
error:
	nl_close(sk);
	nl_socket_free(sk);

	return err;
}

int nl_link_del(const char* if_name){
	struct rtnl_link *link;
	struct nl_sock *sk;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	link = rtnl_link_alloc();
	if (link == NULL){
		err = NLE_NOMEM;
		nl_perror(err, "Unable to allocate interface");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	rtnl_link_set_name(link, if_name);

	if ((err = rtnl_link_delete(sk, link)) < 0) {
		nl_perror(err, "Unable to delete interface");
		rtnl_link_put(link);
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);
	return NLE_SUCCESS;
}

int nl_link_set_up(const char* if_name){
	struct rtnl_link *link;
	struct nl_sock *sk;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	struct nl_cache *cache;
	if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache)) < 0){
		nl_perror(err, "Unable to allocate cache");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	link = rtnl_link_get_by_name(cache, if_name);
	if (!link){
		err = NLE_OBJ_NOTFOUND;
		nl_perror(err, "Unable to modify interface");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	rtnl_link_set_flags(link, IFF_UP);
	err = rtnl_link_change(sk, link, link, NLM_F_REQUEST);
	if (err < 0){
		nl_perror(err, "Unable to modify interface");
		nl_cache_free(cache);
		rtnl_link_put(link);
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	nl_cache_free(cache);
	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);

	return NLE_SUCCESS;
}

int nl_link_read_addr(const char* if_name, const char* mac)
{
	struct rtnl_link *link;
	struct nl_sock *sk;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	struct nl_cache *cache;
	if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache)) < 0){
		nl_perror(err, "Unable to allocate cache");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	/*
	 * print mac addres
	 */
	struct nl_addr *addr_gen;
	link = rtnl_link_get_by_name(cache, if_name);
	addr_gen = rtnl_link_get_addr(link);
	char *res = calloc(1, 1000);
	nl_addr2str(addr_gen, res, 1000);
	printf("mac: %s\n", res);

	nl_cache_free(cache);
	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);

	return NLE_SUCCESS;
}

int nl_link_assign_addr(const char* if_name, const char* mac)
{
	struct rtnl_link *link;
	struct nl_sock *sk;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	struct nl_cache *cache;
	if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache)) < 0){
		nl_perror(err, "Unable to allocate cache");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	/*
	link = rtnl_link_get_by_name(cache, if_name);
	if (!link){
		err = NLE_OBJ_NOTFOUND;
		nl_perror(err, "Unable to modify interface");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}
	*/


	link = rtnl_link_get_by_name(cache, if_name);
	struct nl_addr *addr;
	addr = nl_addr_alloc(IFNAMSIZ);
	err = nl_addr_parse(mac, AF_LLC, &addr);
	if (err < 0){
		printf("parse error\n");
	}
	rtnl_link_set_addr(link, addr);
	err = rtnl_link_change(sk, link, link, NLM_F_REQUEST);
	if (err < 0){
		nl_perror(err, "Unable to modify interface");
		nl_cache_free(cache);
		rtnl_link_put(link);
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}


	/*
	addr = rtnl_addr_alloc();
	int link_index = rtnl_link_name2i(cache, if_name);
	rtnl_addr_set_ifindex(addr, link_index);
	rtnl_addr_set_local(addr, addr);
	rtnl_addr_set_label(addr, "rrrrrRRRRR");
	rtnl_addr_add(sk, addr, 0);
	*/


	
	nl_cache_free(cache);
	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);

	return NLE_SUCCESS;
}

int nl_link_ingress_add_qdisc(const char* if_name)
{
	struct rtnl_link *link;
	struct nl_sock *sk;
	struct rtnl_qdisc *qdisc;
	int err;

	sk = nl_socket_alloc();
	if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
		nl_perror(err, "Unable to connect socket");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	struct nl_cache *cache;
	if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache)) < 0){
		nl_perror(err, "Unable to allocate cache");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	link = rtnl_link_get_by_name(cache, if_name);
	if (!link){
		err = NLE_OBJ_NOTFOUND;
		nl_perror(err, "Unable to modify interface");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	qdisc = rtnl_qdisc_alloc();
	if (!qdisc) {
		err = -1; // TODO
		nl_perror(err, "Unable to create qdisc");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}
	rtnl_tc_set_parent(TC_CAST(qdisc), TC_H_INGRESS); // TODO error handling
	rtnl_tc_set_handle(TC_CAST(qdisc), 0xFFFF0000);   // TODO error handling
	rtnl_qdisc_add(sk, qdisc, NLM_F_REPLACE);  // TODO error handling
	
	nl_cache_free(cache);
	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);
	rtnl_qdisc_put(qdisc);

	return NLE_SUCCESS;
}
