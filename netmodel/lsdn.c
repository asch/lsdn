#include "include/lsdn.h"
#include "private/nl.h"
#include "private/net.h"
#include <errno.h>

extern struct lsdn_net_ops lsdn_net_vlan_ops;
static struct lsdn_net_ops *net_ops_list[] = {
	NULL, &lsdn_net_vlan_ops, NULL
};

static struct lsdn_net_ops *net_ops(struct lsdn_net *net)
{
	return net_ops_list[net->nettype];
}

struct lsdn_context *lsdn_context_new(const char* name)
{
	struct lsdn_context *ctx = malloc(sizeof(*ctx));
	if(!ctx)
		return NULL;

	// TODO: restrict the maximum name length
	ctx->name = strdup(name);
	if(!ctx->name){
		free(ctx);
		return NULL;
	}

	ctx->nlsock = lsdn_socket_init();
	if(!ctx->nlsock){
		free(ctx->name);
		free(ctx);
		return NULL;
	}

	ctx->ifcount = 0;
	lsdn_list_init(&ctx->networks_list);
	lsdn_list_init(&ctx->phys_list);
	return ctx;
}

void lsdn_context_free(struct lsdn_context *ctx)
{
	// TODO: cleanup the name, context, socket and all children (nets and physes)
}

struct lsdn_net *lsdn_net_new_vlan(struct lsdn_context *ctx, uint32_t vlan_id)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		return NULL;
	net->ctx = ctx;
	net->switch_type = LSDN_LEARNING;
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
	phys->is_local = false;
	lsdn_list_init_add(&ctx->phys_list, &phys->phys_entry);
	lsdn_list_init(&phys->attached_to_list);
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

	if(net->switch_type == LSDN_LEARNING) {
		lsdn_init_if(&a->bridge.bridge_if);
		lsdn_init_if(&a->bridge.tunnel_if);
	}

	// TODO: do nettype specific setup here possibly only if local
	return LSDNE_OK;
}

lsdn_err_t lsdn_phys_set_iface(struct lsdn_phys *phys, const char *iface){
	char* iface_dup = strdup(iface);
	if(iface_dup == NULL)
		return LSDNE_NOMEM;

	free(phys->attr_iface);
	phys->attr_iface = iface_dup;
	return LSDNE_OK;
}

lsdn_err_t lsdn_phys_clear_iface(struct lsdn_phys *phys){
	if(phys->is_local)
		return LSDNE_MISSING_ATTR;

	free(phys->attr_iface);
	phys->attr_iface = NULL;
	return LSDNE_OK;
}

static bool is_phys_used(struct lsdn_phys *phys)
{
	lsdn_foreach(phys->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, a) {
		if(!lsdn_is_list_empty(&a->connected_virt_list))
			return true;
	}
	return false;
}

lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys)
{
	if(is_phys_used(phys))
		return LSDNE_INVALID_STATE;
	if(!phys->attr_iface)
		return LSDNE_MISSING_ATTR;

	phys->is_local = true;

	return LSDNE_OK;
}
struct lsdn_virt *lsdn_virt_new(struct lsdn_net *net){
	struct lsdn_virt *virt = malloc(sizeof(*virt));
	if(!virt)
		return NULL;
	virt->network = net;
	virt->attr_mac = NULL;
	virt->connected_through = NULL;
	lsdn_init_if(&virt->connected_if);
	lsdn_list_init_add(&net->virt_list, &virt->virt_entry);
	lsdn_list_init(&virt->virt_rules_list);

	return virt;
}

lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt *virt, struct lsdn_phys *phys, const char *iface)
{
	struct lsdn_phys_attachment *a = find_attachement(phys, virt->network);
	if(!a)
		return LSDNE_NOT_ATTACHED;

	struct lsdn_if new_if;
	lsdn_init_if(&new_if);
	if(phys->is_local){
		int err = lsdn_if_by_name(&new_if, iface);
		if(err != LSDNE_OK)
			return err;
	}

	lsdn_virt_disconnect(virt);
	virt->connected_if = new_if;
	virt->connected_through = a;
	lsdn_list_init_add(&a->connected_virt_list, &virt->connected_virt_entry);

	return LSDNE_OK;
}

void lsdn_virt_disconnect(struct lsdn_virt *virt){
	if(!virt->connected_through)
		return;

	lsdn_list_remove(&virt->connected_virt_entry);
	virt->connected_through = NULL;
}

const char *lsdn_mk_ifname(struct lsdn_context* ctx)
{
	snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s-%d", ctx->name, ++ctx->ifcount);
	return ctx->namebuf;
}

static void add_virt_to_bridge(
	struct lsdn_phys_attachment *a,
	struct lsdn_if *br, struct lsdn_virt *v)
{
	int err = lsdn_link_set_master(a->net->ctx->nlsock, br->ifindex, v->connected_if.ifindex);
	if(err)
		abort();
}

static void commit_attachment(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;
	if(a->net->switch_type == LSDN_LEARNING && !lsdn_if_created(&a->bridge.bridge_if)){
		struct lsdn_if bridge_if;
		lsdn_init_if(&bridge_if);

		int err = lsdn_link_bridge_create(ctx->nlsock, &bridge_if, lsdn_mk_ifname(ctx));
		if(err){
			abort();
		}

		lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
			add_virt_to_bridge(a, &bridge_if, v);
		}

		net_ops(a->net)->mktun_br(a);
		err = lsdn_link_set_master(
			ctx->nlsock, bridge_if.ifindex, a->bridge.tunnel_if.ifindex);
		if(err)
			abort();

		err = lsdn_link_set(ctx->nlsock, a->bridge.tunnel_if.ifindex, true);
		if(err)
			abort();

		err = lsdn_link_set(ctx->nlsock, bridge_if.ifindex, true);
		if(err)
			abort();

		a->bridge.bridge_if = bridge_if;
	}
}

void lsdn_commit(struct lsdn_context *ctx)
{
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if(p->is_local){
			lsdn_foreach(
				p->attached_to_list, attached_to_entry,
				struct lsdn_phys_attachment, a)
			{
				commit_attachment(a);
			}
		}
	}
}
