#include "lsdn.h"

static struct lsdn_context *ctx;
static struct lsdn_settings *settings;
static struct lsdn_net *vxlan1, *vxlan2;
static struct lsdn_phys *phys_a, *phys_b;
static struct lsdn_virt *virt_a1, *virt_a2, *virt_a3, *virt_b1, *virt_b2;
static lsdn_ip_t mcast_ip = { .v = LSDN_IPv4, .v4 = { .bytes = { 239, 239, 239, 239 } } };

static lsdn_ip_t phys_a_ip = { .v = LSDN_IPv4, .v4 = { .bytes = { 172, 16, 0, 1 } } };
static lsdn_ip_t phys_b_ip = { .v = LSDN_IPv4, .v4 = { .bytes = { 172, 16, 0, 2 } } };

static uint16_t port = 4789;

int main(int argc, const char* argv[])
{
	assert(argc == 2);
	int local_phys = atoi(argv[1]);

	ctx = lsdn_context_new("ls");
	settings = lsdn_settings_new_vxlan_mcast(ctx, mcast_ip, port);
	vxlan1 = lsdn_net_new(settings, 0x0B0B);
	vxlan2 = lsdn_net_new(settings, 0x0B0A);

	phys_a = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_a, vxlan1);
	lsdn_phys_attach(phys_a, vxlan2);
	lsdn_phys_set_iface(phys_a, "out");
	lsdn_phys_set_ip(phys_a, &phys_a_ip);

	phys_b = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_b, vxlan1);
	lsdn_phys_attach(phys_b, vxlan2);
	lsdn_phys_set_iface(phys_b, "out");
	lsdn_phys_set_ip(phys_b, &phys_b_ip);

	lsdn_phys_claim_local(local_phys == 0 ? phys_a : phys_b);

	virt_a1 = lsdn_virt_new(vxlan1);
	lsdn_virt_connect(virt_a1, phys_a, "1");

	virt_a2 = lsdn_virt_new(vxlan1);
	lsdn_virt_connect(virt_a2, phys_a, "2");

	virt_a3 = lsdn_virt_new(vxlan2);
	lsdn_virt_connect(virt_a3, phys_a, "3");

	virt_b1 = lsdn_virt_new(vxlan1);
	lsdn_virt_connect(virt_b1, phys_b, "1");

	virt_b2 = lsdn_virt_new(vxlan2);
	lsdn_virt_connect(virt_b2, phys_b, "2");

	lsdn_commit(ctx, lsdn_problem_stderr_handler, NULL);

	lsdn_context_free(ctx);
	return 0;
}
