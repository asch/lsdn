#include "include/lsdn.h"
#include "private/nl.h"
#include "private/net.h"
#include <errno.h>


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
	lsdn_names_init(&ctx->phys_names);
	lsdn_names_init(&ctx->net_names);
	lsdn_list_init(&ctx->networks_list);
	lsdn_list_init(&ctx->settings_list);
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
	// TODO: call shutdown hooks
}

void lsdn_settings_register_user_hooks(
        struct lsdn_settings *settings, struct lsdn_user_hooks *user_hooks)
{
	if (!settings)
		return;
	settings->user_hooks = user_hooks;
}

struct lsdn_net *lsdn_net_new(struct lsdn_settings *s, uint32_t vnet_id)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		ret_ptr(s->ctx, NULL);

	net->ctx = s->ctx;
	net->settings = s;
	net->vnet_id = vnet_id;

	lsdn_list_init_add(&s->ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	lsdn_name_init(&net->name);
	return net;
}

lsdn_err_t lsdn_net_set_name(struct lsdn_net *net, const char *name)
{
	ret_err(net->ctx, lsdn_name_set(&net->name, &net->ctx->net_names, name));
}

const char* lsdn_net_get_name(struct lsdn_net *net)
{
	return net->name.str;
}

struct lsdn_net* lsdn_net_by_name(struct lsdn_context *ctx, const char *name)
{
	struct lsdn_name *r = lsdn_names_search(&ctx->net_names, name);
	if(!r)
		return NULL;
	return lsdn_container_of(r, struct lsdn_net, name);
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
	lsdn_name_init(&phys->name);
	lsdn_list_init_add(&ctx->phys_list, &phys->phys_entry);
	lsdn_list_init(&phys->attached_to_list);
	ret_ptr(ctx, phys);
}
lsdn_err_t lsdn_phys_set_name(struct lsdn_phys *phys, const char *name)
{
	ret_err(phys->ctx, lsdn_name_set(&phys->name, &phys->ctx->phys_names, name));
}

const char* lsdn_phys_get_name(struct lsdn_phys *phys)
{
	return phys->name.str;
}

struct lsdn_phys* lsdn_phys_by_name(struct lsdn_context *ctx, const char *name)
{
	struct lsdn_name *r = lsdn_names_search(&ctx->phys_names, name);
	if(!r)
		return NULL;
	return lsdn_container_of(r, struct lsdn_phys, name);
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
	lsdn_if_init_empty(&a->bridge_if);
	lsdn_list_init(&a->tunnel_list);

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

lsdn_err_t lsdn_phys_set_ip(struct lsdn_phys *phys, lsdn_ip_t ip)
{
	lsdn_ip_t *ip_dup = malloc(sizeof(*ip_dup));
	if (ip_dup == NULL)
		return LSDNE_NOMEM;
	*ip_dup = ip;

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
	lsdn_list_init(&virt->owned_rules_list);
	lsdn_list_init(&virt->owned_actions_list);

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

lsdn_err_t lsdn_virt_set_mac(struct lsdn_virt *virt, lsdn_mac_t mac)
{
	lsdn_mac_t *mac_dup = malloc(sizeof(*mac_dup));
	if (mac_dup == NULL)
		return LSDNE_NOMEM;
	*mac_dup = mac;

	free(virt->attr_mac);
	virt->attr_mac = mac_dup;
	return LSDNE_OK;
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
		if(pa->net->settings->ops->validate_virt)
			pa->net->settings->ops->validate_virt(v);
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
				if(a->net->settings->ops->validate_pa)
					a->net->settings->ops->validate_pa(a);
				validate_virts(a);
			}
		}
	}

	return (ctx->problem_count == 0) ? LSDNE_OK : LSDNE_VALIDATE;
}

static void trigger_startup_hooks(struct lsdn_context *ctx)
{
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p) {
		if (p->is_local) {
			lsdn_foreach(
				p->attached_to_list, attached_to_entry,
				struct lsdn_phys_attachment, a)
			{
				struct lsdn_settings *s = a->net->settings;
				if (s->user_hooks && s->user_hooks->lsdn_startup_hook)
					s->user_hooks->lsdn_startup_hook(
						a->net, p, s->user_hooks->lsdn_startup_hook_user);
			}
		}
	}
}

lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	trigger_startup_hooks(ctx);

	lsdn_err_t lerr = lsdn_validate(ctx, cb, user);
	if(lerr != LSDNE_OK)
		return lerr;

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if(p->is_local){
			// TODO: Should we do that here? It sort of makes sense, but on the other
			// hand the user might not expect us to play games with his physical interfaces?
			// This could e.g. disconnect a user from a network or something.
			// The IP is probably already setup correctly
			/*if (p->attr_ip) {
				int err = lsdn_link_set_ip(ctx->nlsock, p->attr_iface, p->attr_ip);
				if (err)
					abort();
			}*/

			lsdn_foreach(
				p->attached_to_list, attached_to_entry,
				struct lsdn_phys_attachment, a)
			{
				a->net->settings->ops->create_pa(a);
			}
		}
	}

	return (ctx->problem_count == 0) ? LSDNE_OK : LSDNE_COMMIT;
}
