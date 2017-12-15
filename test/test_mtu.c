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
static struct lsdn_virt *v1;

int main(int argc, const char* argv[])
{
	assert(argc == 1);

	ctx = lsdn_context_new("ls");
	lsdn_context_abort_on_nomem(ctx);
	settings = lsdn_settings_new_vxlan_mcast(ctx, LSDN_MK_IPV4(239,239,239,239), 0);
	net = lsdn_net_new(settings, 1);

	phys = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys, net);
	lsdn_phys_set_iface(phys, "lo");
	lsdn_phys_claim_local(phys);

	v1 = lsdn_virt_new(net);
	lsdn_virt_set_mac(v1, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xa1));
	lsdn_virt_connect(v1, phys, "1");

	unsigned int mtu;
	lsdn_err_t err = lsdn_virt_get_recommended_mtu(v1, &mtu);
	assert(err == LSDNE_OK);
	printf("Mtu is %u\n", mtu);
	/* 36 = ip + udp + vxlan, 14 = inner eth */
	/* 65536 is the default loopback mtu, hope nobody has changed that */
	assert(mtu == 65536 - 36 - 14);
	lsdn_context_free(ctx);
	return 0;
}
