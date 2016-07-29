#include "../netmodel/private/nl.h"
#include "../netmodel/include/util.h"
#include <assert.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/tc_act/tc_gact.h>

void mirred(struct mnl_socket *sock, int from, int to, int handle, int action)
{
	struct lsdn_filter *filter = lsdn_filter_init("flower", from,
				1, handle, 1, ETH_P_ALL<<8 /* magic trick 1 */);

	lsdn_filter_actions_start(filter, TCA_FLOWER_ACT);
	lsdn_action_mirred_add(filter, 1, TC_ACT_STOLEN, action, to);
	lsdn_filter_actions_end(filter);

	int err = lsdn_filter_create(sock, filter);
	assert(err == 0);

	lsdn_filter_free(filter);
}

/*
 *
 *
 */

int main(int argc, const char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);


	int ltif1 = if_nametoindex("ltif1");
	assert(ltif1 != 0);
	int ltif2 = if_nametoindex("ltif2");
	assert(ltif2 != 0);

	int ret;

	struct mnl_socket* sock = lsdn_socket_init();

	ret = lsdn_qdisc_ingress_create(sock, ltif2);
	assert(ret == 0);
	ret = lsdn_qdisc_htb_create(sock, ltif1,
				TC_H_ROOT, LSDN_DEFAULT_EGRESS_HANDLE, 10, 0);
	assert(ret == 0);

	mirred(sock, ltif2, ltif1, LSDN_INGRESS_HANDLE, TCA_INGRESS_REDIR);
	mirred(sock, ltif1, ltif2, LSDN_DEFAULT_EGRESS_HANDLE, TCA_EGRESS_REDIR);


	lsdn_socket_free(sock);
}
