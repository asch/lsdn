#include "lsdn.h"

static struct lsdn_context *ctx;
static struct lsdn_net *vlan;
static struct lsdn_phys *phys_a, *phys_b;
static struct lsdn_virt *virt_a1, *virt_a2, *virt_b1;

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
	vlan = lsdn_net_new_vlan(ctx, true, 0x0B0B); not_null(vlan);
	phys_a = lsdn_phys_new(ctx); not_null(phys_a);
	phys_b = lsdn_phys_new(ctx); not_null(phys_b);
	err = lsdn_phys_attach(phys_a, vlan); not_err(err);
	err = lsdn_phys_attach(phys_b, vlan); not_err(err);

	virt_a1 = lsdn_virt_new(vlan); not_null(virt_a1);
	virt_a2 = lsdn_virt_new(vlan); not_null(virt_a2);
	virt_b1 = lsdn_virt_new(vlan); not_null(virt_b1);
	err = lsdn_virt_connect(virt_a1, phys_a, "v1"); not_err(err);
	err = lsdn_virt_connect(virt_a2, phys_a, "v2"); not_err(err);
	err = lsdn_virt_connect(virt_a1, phys_a, "v1"); not_err(err);

	err = lsdn_phys_claim_local(local_phys == 0 ? phys_a : phys_b); not_err(err);
	lsdn_commit(ctx);

	lsdn_context_free(ctx);
	return 0;
}
