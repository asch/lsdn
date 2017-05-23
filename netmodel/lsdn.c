#include "include/lsdn.h"

struct lsdn_context *lsdn_context_new(const char* name)
{
	struct lsdn_context *ctx = malloc(sizeof(*ctx));
	if(!ctx)
		goto err;
	ctx->name = strdup(name);
	if(!ctx->name)
		goto err;

	lsdn_list_init(&ctx->networks_list);
	lsdn_list_init(&ctx->phys_list);
	return ctx;

	err:
	free(ctx);
	return NULL;
}

void lsdn_context_free(struct lsdn_context *ctx)
{
	// TODO: cleanup the name, context and all children (nets and physes)
}

struct lsdn_net *lsdn_net_new_vlan(
	struct lsdn_context *ctx,
	enum lsdn_switch switch_type, uint32_t vlan_id)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		return NULL;
	net->ctx = ctx;
	net->switch_type = switch_type;
	net->nettype = LSDN_NET_VLAN;
	net->vlan_id = vlan_id;
	lsdn_list_init_add(&ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	return net;
}

struct lsdn_phys *lsdn_phys_new(struct lsdn_context *ctx)
{
	struct lsdn_phys *phys = malloc(sizeof(*phys));
	if(!phys)
		return NULL;
	phys->ctx = ctx;
	phys->attr_iface = NULL;
	phys->attr_ip = NULL;
	lsdn_list_init_add(&ctx->phys_list, &phys->phys_entry);
	lsdn_list_init(&ctx->phys_list);
	return phys;
}

static struct lsdn_phys_attachment* find_attachement(
	struct lsdn_phys *phys, struct lsdn_net* net)
{
	lsdn_foreach(phys->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, a) {
		if(a->net == net)
			return a;
	}
	return NULL;
}

lsdn_err_t lsdn_phys_attach(struct lsdn_phys *phys, struct lsdn_net* net)
{
	struct lsdn_phys_attachment *a = find_attachement(phys, net);
	if(a)
		return LSDNE_OK;

	a = malloc(sizeof(*a));
	if(!a)
		return LSDNE_NOMEM;

	a->net = net;
	a->phys = phys;
	lsdn_list_init_add(&net->attached_list, &a->attached_entry);
	lsdn_list_init_add(&phys->attached_to_list, &a->attached_to_entry);
	lsdn_list_init(&a->connected_virt_list);
	// TODO: do nettype specific setup here possibly only if local
	return LSDNE_OK;
}

lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys)
{
	if(!phys->attr_iface)
		return LSDNE_MISSING_ATTR;

	return LSDNE_OK;
}
struct lsdn_virt *lsdn_virt_new(struct lsdn_net *ctx){
	// TODO
	return NULL;
}

lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt* virt, struct lsdn_phys* phys, const char* iface)
{
	struct lsdn_phys_attachment *a = find_attachement(phys, virt->network);
	if(!a)
		return LSDNE_NOT_ATTACHED;
	// TODO
	return LSDNE_OK;
}

void lsdn_commit(struct lsdn_context *ctx)
{

}
