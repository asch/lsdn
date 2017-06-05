#include "lsdn.h"

static struct lsdn_context *ctx;
static struct lsdn_net *vxlan1, *vxlan2;
static struct lsdn_phys *phys_a, *phys_b;
static struct lsdn_virt *virt_a1, *virt_a2, *virt_a3, *virt_b1, *virt_b2;
static lsdn_ip_t mcast_ip = { .v = LSDN_IPv4, .v4 = { .bytes = { 239, 239, 239, 239 } } };

static lsdn_ip_t phys_a_ip = { .v = LSDN_IPv4, .v4 = { .bytes = { 172, 16, 0, 1 } } };
static lsdn_ip_t phys_b_ip = { .v = LSDN_IPv4, .v4 = { .bytes = { 172, 16, 0, 2 } } };

static uint16_t port = 4789;

static inline void not_null(void* p){
	if(p == NULL)
		abort();
}

static inline void not_err(lsdn_err_t err){
	if(err != LSDNE_OK)
		abort();
}

int main(int argc, const char* argv[])
{
	lsdn_err_t err;
	assert(argc == 2);
	int local_phys = atoi(argv[1]);

	ctx = lsdn_context_new("ls"); not_null(ctx);
	vxlan1 = lsdn_net_new_vxlan_mcast(ctx, 0x0B0B, mcast_ip, port); not_null(vxlan1);
	vxlan2 = lsdn_net_new_vxlan_mcast(ctx, 0x0B0A, mcast_ip, port); not_null(vxlan1);

	phys_a = lsdn_phys_new(ctx); not_null(phys_a);
	err = lsdn_phys_attach(phys_a, vxlan1); not_err(err);
	err = lsdn_phys_attach(phys_a, vxlan2); not_err(err);
	err = lsdn_phys_set_iface(phys_a, "out"); not_err(err);
	err = lsdn_phys_set_ip(phys_a, &phys_a_ip); not_err(err);

	phys_b = lsdn_phys_new(ctx); not_null(phys_b);
	err = lsdn_phys_attach(phys_b, vxlan1); not_err(err);
	err = lsdn_phys_attach(phys_b, vxlan2); not_err(err);
	err = lsdn_phys_set_iface(phys_b, "out"); not_err(err);
	err = lsdn_phys_set_ip(phys_b, &phys_b_ip); not_err(err);

	err = lsdn_phys_claim_local(local_phys == 0 ? phys_a : phys_b); not_err(err);

	virt_a1 = lsdn_virt_new(vxlan1); not_null(virt_a1);
	err = lsdn_virt_connect(virt_a1, phys_a, "1"); not_err(err);

	virt_a2 = lsdn_virt_new(vxlan1); not_null(virt_a2);
	err = lsdn_virt_connect(virt_a2, phys_a, "2"); not_err(err);

	virt_a3 = lsdn_virt_new(vxlan2); not_null(virt_a3);
	err = lsdn_virt_connect(virt_a3, phys_a, "3"); not_err(err);

	virt_b1 = lsdn_virt_new(vxlan1); not_null(virt_b1);
	err = lsdn_virt_connect(virt_b1, phys_b, "1"); not_err(err);

	virt_b2 = lsdn_virt_new(vxlan2); not_null(virt_b2);
	err = lsdn_virt_connect(virt_b2, phys_b, "2"); not_err(err);

	lsdn_commit(ctx);

	lsdn_context_free(ctx);
	return 0;
}
