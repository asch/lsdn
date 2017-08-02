#include "lsdn.h"

static struct lsdn_context *ctx;
static struct lsdn_settings *settings;
static struct lsdn_net *vlan1, *vlan2;
static struct lsdn_phys *phys_a, *phys_b, *phys_c;
static struct lsdn_virt
	*dhcp_server,
	*virt_b1, *virt_b2, *virt_b3,
	*virt_c1, *virt_c2;


int main(int argc, const char* argv[])
{
	assert(argc == 2);
	int local_phys = atoi(argv[1]);

	ctx = lsdn_context_new("ls");
	lsdn_context_abort_on_nomem(ctx);
	settings = lsdn_settings_new_vlan(ctx);
	vlan1 = lsdn_net_new(settings, 0x0B0B);
	vlan2 = lsdn_net_new(settings, 0x0B0A);

	phys_a = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_a, vlan1);
	lsdn_phys_set_iface(phys_a, "out");

	phys_b = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_b, vlan1);
	lsdn_phys_attach(phys_b, vlan2);
	lsdn_phys_set_iface(phys_b, "out");

	phys_c = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_c, vlan1);
	lsdn_phys_attach(phys_c, vlan2);
	lsdn_phys_set_iface(phys_c, "out");

	switch (local_phys) {
	case 0:
		lsdn_phys_claim_local(phys_a);
		break;
	case 1:
		lsdn_phys_claim_local(phys_b);
		break;
	case 2:
		lsdn_phys_claim_local(phys_c);
		break;
	default:
		abort();
	}

	dhcp_server = lsdn_virt_new(vlan1);
	lsdn_virt_connect(dhcp_server, phys_a, "1");

	virt_b1 = lsdn_virt_new(vlan1);
	lsdn_virt_connect(virt_b1, phys_b, "1");

	virt_b2 = lsdn_virt_new(vlan1);
	lsdn_virt_connect(virt_b2, phys_b, "2");

	virt_b3 = lsdn_virt_new(vlan2);
	lsdn_virt_connect(virt_b3, phys_b, "3");

	virt_c1 = lsdn_virt_new(vlan1);
	lsdn_virt_connect(virt_c1, phys_c, "1");

	virt_c2 = lsdn_virt_new(vlan2);
	lsdn_virt_connect(virt_c2, phys_c, "2");

	lsdn_commit(ctx, lsdn_problem_stderr_handler, NULL);

	lsdn_context_free(ctx);
	return 0;
}
