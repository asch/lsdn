#include <lsdn.h>
#include <rules.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

static struct lsdn_context *ctx;
static struct lsdn_settings *settings;
static struct lsdn_net *net;
static struct lsdn_phys *phys;
static struct lsdn_virt *v1, *v2;

int main(int argc, const char* argv[])
{
	assert(argc == 1);

	ctx = lsdn_context_new("ls");
	lsdn_context_abort_on_nomem(ctx);
	settings = settings_from_env(ctx);
	net = lsdn_net_new(settings, 1);
	phys = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys, net);
	lsdn_phys_set_iface(phys, "out");
	lsdn_phys_set_ip(phys, LSDN_MK_IPV4(172, 16, 0, 1));
	lsdn_phys_claim_local(phys);

	v1 = lsdn_virt_new(net);
	lsdn_virt_set_mac(v1, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xa1));
	lsdn_virt_connect(v1, phys, "1");

	lsdn_vr_new_src_ip(v1, 0, LSDN_IN, LSDN_MK_IPV4(192, 168, 99, 2), &LSDN_VR_DROP);
	lsdn_vr_new_dst_ip(v1, 0, LSDN_OUT, LSDN_MK_IPV4(192, 168, 99, 3), &LSDN_VR_DROP);

	v2 = lsdn_virt_new(net);
	lsdn_virt_set_mac(v2, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xa2));
	lsdn_virt_connect(v2, phys, "2");

	lsdn_commit(ctx, lsdn_problem_stderr_handler, NULL);

	lsdn_context_free(ctx);
	return 0;
}
