#include "include/lsdn.h"
#include "private/nl.h"
#include "private/net.h"
#include <errno.h>

static lsdn_err_t ret_err(struct lsdn_context *ctx, lsdn_err_t err)
{
	if(err == LSDNE_NOMEM && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return err;
}
#define ret_err(ctx, x) return ret_err(ctx, x)

static void *ret_ptr(struct lsdn_context *ctx, void *ptr){
	if(ptr == NULL && ctx->nomem_cb)
		ctx->nomem_cb(ctx->nomem_cb_user);
	return ptr;
}
#define ret_ptr(ctx, x) return ret_ptr(ctx, x)

struct lsdn_context *lsdn_context_new(const char* name)
{
	struct lsdn_context *ctx = malloc(sizeof(*ctx));
	if(!ctx)
		return NULL;

	ctx->nomem_cb = NULL;
	ctx->nomem_cb_user = NULL;

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

void lsdn_context_set_nomem_callback(struct lsdn_context *ctx, lsdn_nomem_cb cb, void *user)
{
	if(ctx == NULL)
		cb(user);

	ctx->nomem_cb = cb;
	ctx->nomem_cb_user = user;
}

static void lsdn_abort_cb(void *user)
{
	fprintf(stderr, "liblsdn: Can not allocate memory\n");
	abort();
}

void lsdn_context_abort_on_nomem(struct lsdn_context *ctx)
{
	lsdn_context_set_nomem_callback(ctx, lsdn_abort_cb, NULL);
}

void lsdn_context_free(struct lsdn_context *ctx)
{
	// TODO: cleanup the name, context, socket and all children (nets and physes)
}

struct lsdn_phys *lsdn_phys_new(struct lsdn_context *ctx)
{
	struct lsdn_phys *phys = malloc(sizeof(*phys));
	if(!phys)
		ret_ptr(ctx, NULL);

	phys->ctx = ctx;
	phys->attr_iface = NULL;
	phys->attr_ip = NULL;
	phys->is_local = false;
	lsdn_list_init_add(&ctx->phys_list, &phys->phys_entry);
	lsdn_list_init(&phys->attached_to_list);
	ret_ptr(ctx, phys);
}

static struct lsdn_phys_attachment* find_or_create_attachement(
	struct lsdn_phys *phys, struct lsdn_net* net)
{
	lsdn_foreach(phys->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, a) {
		if(a->net == net)
			return a;
	}

	struct lsdn_phys_attachment *a = malloc(sizeof(*a));
	if(!a)
		return NULL;

	a->phys = phys;
	a->net = net;

	lsdn_list_init_add(&net->attached_list, &a->attached_entry);
	lsdn_list_init_add(&phys->attached_to_list, &a->attached_to_entry);
	lsdn_list_init(&a->connected_virt_list);
	a->explicitely_attached = false;
	return a;
}

lsdn_err_t lsdn_phys_attach(struct lsdn_phys *phys, struct lsdn_net* net)
{
	struct lsdn_phys_attachment *a = find_or_create_attachement(phys, net);
	if(!a)
		ret_err(net->ctx, LSDNE_NOMEM);

	if(a->explicitely_attached)
		return LSDNE_OK;

	a->explicitely_attached = true;
	if(net->switch_type == LSDN_LEARNING || net->switch_type == LSDN_LEARNING_E2E) {
		lsdn_if_init_empty(&a->bridge.bridge_if);
		lsdn_if_init_empty(&a->bridge.tunnel_if);
	}

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
	free(phys->attr_iface);
	phys->attr_iface = NULL;
	return LSDNE_OK;
}

lsdn_err_t lsdn_phys_set_ip(struct lsdn_phys *phys, const lsdn_ip_t *ip)
{
	lsdn_ip_t *ip_dup = malloc(sizeof(*ip_dup));
	if (ip_dup == NULL)
		return LSDNE_NOMEM;
	*ip_dup = *ip;

	free(phys->attr_ip);
	phys->attr_ip = ip_dup;
	return LSDNE_OK;
}

lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys)
{
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
	lsdn_if_init_empty(&virt->connected_if);
	lsdn_list_init_add(&net->virt_list, &virt->virt_entry);
	lsdn_list_init(&virt->virt_rules_list);

	return virt;
}

lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt *virt, struct lsdn_phys *phys, const char *iface)
{
	struct lsdn_phys_attachment *a = find_or_create_attachement(phys, virt->network);
	if(!a)
		ret_err(phys->ctx, LSDNE_NOMEM);

	struct lsdn_if new_if;
	lsdn_err_t err = lsdn_if_init_name(&new_if, iface);
	if(err != LSDNE_OK)
		ret_err(phys->ctx, err);

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

static void report_virts(struct lsdn_phys_attachment *pa)
{
	lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v)
	{
		lsdn_problem_report(pa->net->ctx, LSDNP_PHYS_NOT_ATTACHED,
			LSDNS_VIRT, v,
			LSDNS_NET, pa->net,
			LSDNS_PHYS, pa->phys,
			LSDNS_END);
	}
}

static void validate_virts(struct lsdn_phys_attachment *pa)
{
	lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
		if(v->connected_through && pa->explicitely_attached
		   && v->connected_through->phys->is_local)
		{
			lsdn_err_t err = lsdn_if_prepare(&v->connected_if);
			if(err != LSDNE_OK)
				lsdn_problem_report(
					v->network->ctx, LSDNP_VIRT_NOIF,
					LSDNS_IF, &v->connected_if,
					LSDNS_VIRT, v, LSDNS_END);
		}
		if(pa->net->ops->validate_virt)
			pa->net->ops->validate_virt(v);
	}
}

lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	ctx->problem_cb = cb;
	ctx->problem_cb_user = user;
	ctx->problem_count = 0;

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		lsdn_foreach(p->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, a)
		{
			if(!a->explicitely_attached){
				report_virts(a);
			}else{
				if(a->net->ops->validate_pa)
					a->net->ops->validate_pa(a);
				validate_virts(a);
			}
		}
	}

	return (ctx->problem_count == 0) ? LSDNE_OK : LSDNE_VALIDATE;
}

static void commit_attachment(struct lsdn_phys_attachment *a)
{
	struct lsdn_context *ctx = a->net->ctx;
	if((a->net->switch_type == LSDN_LEARNING || a->net->switch_type == LSDN_LEARNING_E2E)
			&& !lsdn_if_is_set(&a->bridge.bridge_if)) {
		struct lsdn_if bridge_if;
		lsdn_if_init_empty(&bridge_if);

		int err = lsdn_link_bridge_create(ctx->nlsock, &bridge_if, lsdn_mk_ifname(ctx));
		if(err){
			abort();
		}

		lsdn_foreach(a->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
			add_virt_to_bridge(a, &bridge_if, v);
		}

		a->net->ops->mktun_br(a);
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

lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	lsdn_err_t lerr = lsdn_validate(ctx, cb, user);
	if(lerr != LSDNE_OK)
		return lerr;

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if(p->is_local){
			// TODO: Should we do that here? It sort of makes sense, but on the other
			// hand the user might not expect us to play games with his physical interfaces?
			// This could e.g. disconnect a user from a network or something.
			// The IP is probably already setup correctly
			if (p->attr_ip) {
				int err = lsdn_link_set_ip(ctx->nlsock, p->attr_iface, p->attr_ip);
				if (err)
					abort();
			}

			lsdn_foreach(
				p->attached_to_list, attached_to_entry,
				struct lsdn_phys_attachment, a)
			{
				commit_attachment(a);
			}
		}
	}

	return (ctx->problem_count == 0) ? LSDNE_OK : LSDNE_COMMIT;
}
