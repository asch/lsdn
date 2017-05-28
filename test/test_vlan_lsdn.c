#include "lsdn.h"

static struct lsdn_context *ctx;
static struct lsdn_net *vlan1, *vlan2;
static struct lsdn_phys *phys_a, *phys_b;
static struct lsdn_virt *virt_a1, *virt_a2, *virt_a3, *virt_b1, *virt_b2;

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
	vlan1 = lsdn_net_new_vlan(ctx, 0x0B0B); not_null(vlan1);
	vlan2 = lsdn_net_new_vlan(ctx, 0x0B0A); not_null(vlan2);

	phys_a = lsdn_phys_new(ctx); not_null(phys_a);
	err = lsdn_phys_attach(phys_a, vlan1); not_err(err);
	err = lsdn_phys_attach(phys_a, vlan2); not_err(err);
	err = lsdn_phys_set_iface(phys_a, "out"); not_err(err);

	phys_b = lsdn_phys_new(ctx); not_null(phys_b);
	err = lsdn_phys_attach(phys_b, vlan1); not_err(err);
	err = lsdn_phys_attach(phys_b, vlan2); not_err(err);
	err = lsdn_phys_set_iface(phys_b, "out"); not_err(err);

	err = lsdn_phys_claim_local(local_phys == 0 ? phys_a : phys_b); not_err(err);

	virt_a1 = lsdn_virt_new(vlan1); not_null(virt_a1);
	err = lsdn_virt_connect(virt_a1, phys_a, "1"); not_err(err);

	virt_a2 = lsdn_virt_new(vlan1); not_null(virt_a2);
	err = lsdn_virt_connect(virt_a2, phys_a, "2"); not_err(err);

	virt_a3 = lsdn_virt_new(vlan2); not_null(virt_a3);
	err = lsdn_virt_connect(virt_a3, phys_a, "3"); not_err(err);

	virt_b1 = lsdn_virt_new(vlan1); not_null(virt_b1);
	err = lsdn_virt_connect(virt_b1, phys_b, "1"); not_err(err);

	virt_b2 = lsdn_virt_new(vlan2); not_null(virt_b2);
	err = lsdn_virt_connect(virt_b2, phys_b, "2"); not_err(err);

	lsdn_commit(ctx);

	lsdn_context_free(ctx);
	return 0;
}
