#include "nl_link.h"

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/tc.h>
#include <netlink/route/qdisc.h>
#include <netlink/route/class.h>
#include <netlink/route/classifier.h>
#include <netlink/route/addr.h>

#include <linux/if.h>

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
		err = NLE_NOMEM;
		nl_perror(err, "Cannot allocate rtnl_qdisc");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

    rtnl_tc_set_link(TC_CAST(qdisc), link);
    rtnl_tc_set_parent(TC_CAST(qdisc), TC_H_INGRESS);
    rtnl_tc_set_handle(TC_CAST(qdisc), TC_HANDLE(0xfffa, 0));
    err = rtnl_tc_set_kind(TC_CAST(qdisc), "ingress");
    if (err < 0){
        nl_perror(err, "Can not allocate ingress");
		return err;
	}
	err = rtnl_qdisc_add(sk, qdisc, NLM_F_CREATE);
	if (err < 0){
		nl_perror(err, "Unable to add qdisc");
		nl_cache_free(cache);
		rtnl_qdisc_put(qdisc);
		rtnl_link_put(link);
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}
	
	nl_cache_free(cache);
	rtnl_qdisc_put(qdisc);
	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);

	return NLE_SUCCESS;
}

int nl_link_egress_add_qdisc(const char* if_name, uint32_t major, uint32_t minor)
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
		err = NLE_NOMEM;
		nl_perror(err, "Cannot allocate rtnl_qdisc");
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}

	rtnl_tc_set_link(TC_CAST(qdisc), link);
	rtnl_tc_set_parent(TC_CAST(qdisc), TC_H_ROOT);
	rtnl_tc_set_handle(TC_CAST(qdisc), TC_HANDLE(major, minor));
	err = rtnl_tc_set_kind(TC_CAST(qdisc), "htb");
	if (err < 0) {
		nl_perror(err, "Can not allocate egress");
		return err;
	}
	err = rtnl_qdisc_add(sk, qdisc, NLM_F_CREATE);
	if (err < 0) {
		nl_perror(err, "Unable to add qdisc");
		nl_cache_free(cache);
		rtnl_qdisc_put(qdisc);
		rtnl_link_put(link);
		nl_close(sk);
		nl_socket_free(sk);
		return err;
	}
	nl_cache_free(cache);
	rtnl_qdisc_put(qdisc);
	rtnl_link_put(link);
	nl_close(sk);
	nl_socket_free(sk);

	return NLE_SUCCESS;
}

// Create filter hash table
int u32_add_ht(struct nl_sock *sock, struct rtnl_link *rtnlLink,
		uint32_t prio, uint32_t htid, uint32_t divisor){

	int err;
	struct rtnl_cls *cls;

	cls=rtnl_cls_alloc();
	if (!(cls)) {
		printf("Can not allocate classifier\n");
		nl_socket_free(sock);
		exit(1);
	}

	rtnl_tc_set_link(TC_CAST(cls), rtnlLink);

	if ((err = rtnl_tc_set_kind(TC_CAST(cls), "u32"))) {
		printf("Can not set classifier as u32\n");
		return 1;
	}

	rtnl_cls_set_prio(cls, prio);
	rtnl_cls_set_protocol(cls, ETH_P_IP);
	rtnl_tc_set_parent(TC_CAST(cls), TC_HANDLE(0xffff, 0));

	// rtnl_tc_set_handle((struct rtnl_tc *) cls, htid);
	rtnl_u32_set_handle(cls, htid, 0x0, 0x0);
	rtnl_u32_set_divisor(cls, divisor);

	// if ((err = rtnl_cls_add(sock, cls, NLM_F_CREATE | NLM_F_EXCL))) {
	if ((err = rtnl_cls_add(sock, cls, NLM_F_CREATE))) {
		printf("Can not add classifier: %s\n", nl_geterror(err));
		return -1;
	}
	rtnl_cls_put(cls);
	return 0;
}

// Copy pasted from libnl
int get_u32(__u32 *val, const char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0xFFFFFFFFUL)
		return -1;
	*val = res;
	return 0;
}

// Copy pasted from libnl
int get_u32_handle(__u32 *handle, const char *str)
{
	__u32 htid=0, hash=0, nodeid=0;
	char *tmp = strchr(str, ':');
        
	if (tmp == NULL) {
		if (memcmp("0x", str, 2) == 0)
			return get_u32(handle, str, 16);
		return -1;
	}
	htid = strtoul(str, &tmp, 16);
	if (tmp == str && *str != ':' && *str != 0)
		return -1;
	if (htid>=0x1000)
		return -1;
	if (*tmp) {
		str = tmp+1;
		hash = strtoul(str, &tmp, 16);
		if (tmp == str && *str != ':' && *str != 0)
			return -1;
		if (hash>=0x100)
			return -1;
		if (*tmp) {
			str = tmp+1;
			nodeid = strtoul(str, &tmp, 16);
			if (tmp == str && *str != 0)
				return -1;
			if (nodeid>=0x1000)
				return -1;
		}
	}
	*handle = (htid<<20)|(hash<<12)|nodeid;
	return 0;
}

/*
 * Same as: 'tc filter add dev <link> parent <major>:<minor> protocol all prio <prio> \
 *           u32 match u32 <val> <mask> at <off> <act>'
 *    (e.g. act = action mirred egress redirect dev qwer)
 */
// TODO: find out what 'offmask' is good for?
int u32_add_filter_with_action(struct nl_sock *sock, struct rtnl_link *link, uint32_t prio, 
		uint32_t val, uint32_t mask, int off, int offmask, struct rtnl_act *act,
		uint32_t major, uint32_t minor)
{

	struct rtnl_cls *cls;
	int err;

	cls=rtnl_cls_alloc();
	if (!(cls)) {
		printf("Can not allocate classifier\n");
		nl_socket_free(sock);
		exit(1);
	}

	rtnl_tc_set_link(TC_CAST(cls), link);

	if ((err = rtnl_tc_set_kind(TC_CAST(cls), "u32"))) {
		printf("Can not set classifier as u32\n");
		return 1;
	}

	rtnl_cls_set_prio(cls, prio);
	rtnl_cls_set_protocol(cls, ETH_P_ALL);
	rtnl_tc_set_parent(TC_CAST(cls), TC_HANDLE(major, minor));

	rtnl_u32_add_key_uint32(cls, val, mask, off, offmask);

	rtnl_u32_add_action(cls, act);

	if ((err = rtnl_cls_add(sock, cls, NLM_F_CREATE))) {
		printf("Can not add classifier: %s\n", nl_geterror(err));
		return -1;
	}
	rtnl_cls_put(cls);
	return 0;
}
