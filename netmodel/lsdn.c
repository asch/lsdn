/** \file
 * Main library file. */
#include "include/lsdn.h"
#include "private/nl.h"
#include "private/net.h"
#include "private/log.h"
#include "include/util.h"
#include "private/errors.h"
#include <errno.h>

static void settings_do_free(struct lsdn_settings *settings);
static void net_do_free(struct lsdn_net *net);
static void virt_do_free(struct lsdn_virt *virt);
static void phys_detach_by_pa(struct lsdn_phys_attachment *pa);
static void pa_do_free(struct lsdn_phys_attachment *pa);
static void phys_do_free(struct lsdn_phys *pa);

/** Move from OK to a RENEW state.
 * \private
 * Only switch state to RENEW if the state is OK, not NEW. */
static void renew(enum lsdn_state *state) {
	assert (*state != LSDN_STATE_DELETE);
	if (*state == LSDN_STATE_OK)
		*state = LSDN_STATE_RENEW;
}

/** Propagate RENEW state.
 * \private
 * If `from` is slated for renewal and `to` is OK, switch to RENEW too. */
static void propagate(enum lsdn_state *from, enum lsdn_state *to) {
	if (*from == LSDN_STATE_RENEW && *to == LSDN_STATE_OK)
		*to = LSDN_STATE_RENEW;
}

/** Create new LSDN context.
 * Initialize a `lsdn_context` struct, set its name to `name` and configure a netlink socket.
 * The returned struct must be freed by `lsdn_context_free` after use.
 * @param name Context name.
 * @return `NULL` if allocation failed, pointer to new `lsdn_context` otherwise. */
struct lsdn_context *lsdn_context_new(const char* name)
{
	struct lsdn_context *ctx = malloc(sizeof(*ctx));
	if(!ctx)
		return NULL;

	ctx->nomem_cb = NULL;
	ctx->nomem_cb_user = NULL;
	ctx->disable_decommit = false;

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
	lsdn_names_init(&ctx->setting_names);
	lsdn_list_init(&ctx->networks_list);
	lsdn_list_init(&ctx->settings_list);
	lsdn_list_init(&ctx->phys_list);
	return ctx;
}

/** Problem handler that aborts when a problem is found.
 * Used in `lsdn_context_free`. When freeing a context, we can't handle errors
 * meaningfully and we don't expect any errors to happen anyway. Any reported problem
 * will then simply dump description to `stderr` and abort. */
static void abort_handler(const struct lsdn_problem *problem, void *user)
{
	LSDN_UNUSED(user);
	fprintf(stderr, "WARNING: Encountered an error when freeing network\n");
	lsdn_problem_format(stderr, problem);
	abort();
}

/** Free a LSDN context.
 * Deletes the context and all its child objects from memory. Does __not__
 * delete TC rules from kernel tables.
 *
 * Use this before exiting your program.
 * @param ctx Context to free. */
void lsdn_context_free(struct lsdn_context *ctx)
{
	ctx->disable_decommit = true;
	lsdn_context_cleanup(ctx, abort_handler, NULL);
}

/** Clear a LSDN context.
 * Deletes the context and all its child objects from memory. Also deletes
 * configured TC rules from kernel tables.
 *
 * Use this to deinitialize the LSDN context and tear down the virtual network.
 * @param ctx Context to cleanup.
 * @param cb  Problem callback for encountered errors.
 * @param user User data for the problem callback. */
void lsdn_context_cleanup(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p) {
		lsdn_phys_free(p);
	}
	lsdn_foreach(ctx->settings_list, settings_entry, struct lsdn_settings, s) {
		lsdn_settings_free(s);
	}
	lsdn_commit(ctx, cb, user);
	lsdn_socket_free(ctx->nlsock);
	free(ctx->name);
	free(ctx);
}

/** Configure out-of-memory callback.
 * By default, LSDN will return an error code to indicate that an allocation
 * failed. This function allows you to set a callback that gets called to handle
 * this condition instead.
 * @param ctx LSDN context.
 * @param cb Callback function.
 * @param user User data for the callback function. */
void lsdn_context_set_nomem_callback(struct lsdn_context *ctx, lsdn_nomem_cb cb, void *user)
{
	if(ctx == NULL)
		cb(user);

	ctx->nomem_cb = cb;
	ctx->nomem_cb_user = user;
}

/** Abort on out-of-memory problem.
 * Predefined callback for out-of-memory conditions. Prints a message to `stderr`
 * and aborts. */
static void lsdn_abort_cb(void *user)
{
	LSDN_UNUSED(user);
	fprintf(stderr, "liblsdn: Can not allocate memory\n");
	abort();
}

/** Configure the context to abort on out-of-memory.
 * This sets the out-of-memory callback to `lsdn_abort_cb`. If an allocation
 * fails, this will abort the program.
 *
 * It is recommended to use this, unless you have a specific way to handle
 * out-of-memory conditions.
 * @param ctx LSDN context. */
void lsdn_context_abort_on_nomem(struct lsdn_context *ctx)
{
	lsdn_context_set_nomem_callback(ctx, lsdn_abort_cb, NULL);
}

/** Configure user hooks.
 * Associates a #lsdn_user_hooks structure with `settings`. */
void lsdn_settings_register_user_hooks(
        struct lsdn_settings *settings, struct lsdn_user_hooks *user_hooks)
{
	if (!settings)
		return;
	settings->user_hooks = user_hooks;
}

/** Assign a name to settings.
 * @return #LSDNE_OK if the name is successfully set.
 * @return #LSDNE_DUPLICATE if this name is already in use. */
lsdn_err_t lsdn_settings_set_name(struct lsdn_settings *s, const char *name)
{
	ret_err(s->ctx, lsdn_name_set(&s->name, &s->ctx->setting_names, name));
}

/** Get settings name.
 * @return name of the settings struct. */
const char* lsdn_settings_get_name(struct lsdn_settings *s)
{
	return s->name.str;
}

/** Find settings by name.
 * Searches the context for a named #lsdn_settings object and returns it.
 * @param ctx LSDN context.
 * @param name Requested name
 * @return Pointer to #lsdn_settings with this name.
 * @return `NULL` if no settings with that name exist in the context. */
struct lsdn_settings* lsdn_settings_by_name(struct lsdn_context *ctx, const char *name)
{
	struct lsdn_name *r = lsdn_names_search(&ctx->setting_names, name);
	if(!r)
		return NULL;
	return lsdn_container_of(r, struct lsdn_settings, name);
}

/** Delete settings object from memory.
 * Removes the object from list of settings for its context and then frees it.
 * */
static void settings_do_free(struct lsdn_settings *settings)
{
	lsdn_list_remove(&settings->settings_entry);
	lsdn_name_free(&settings->name);
	assert(lsdn_is_list_empty(&settings->setting_users_list));
	free(settings);
}

/** Free settings object.
 * Deletes the settings object and all #lsdn_net objects that use it. */
void lsdn_settings_free(struct lsdn_settings *settings)
{
	lsdn_foreach(settings->setting_users_list, settings_users_entry, struct lsdn_net, net) {
		lsdn_net_free(net);
	}
	free_helper(settings, settings_do_free);
}

struct lsdn_net *lsdn_net_new(struct lsdn_settings *s, uint32_t vnet_id)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		ret_ptr(s->ctx, NULL);

	net->ctx = s->ctx;
	net->state = LSDN_STATE_NEW;
	net->settings = s;
	net->vnet_id = vnet_id;

	lsdn_list_init_add(&s->setting_users_list, &net->settings_users_entry);
	lsdn_list_init_add(&s->ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	lsdn_name_init(&net->name);
	lsdn_names_init(&net->virt_names);
	ret_ptr(s->ctx, net);
}

static void net_do_free(struct lsdn_net *net)
{
	assert(lsdn_is_list_empty(&net->attached_list));
	assert(lsdn_is_list_empty(&net->virt_list));
	lsdn_list_remove(&net->networks_entry);
	lsdn_list_remove(&net->settings_users_entry);
	lsdn_name_free(&net->name);
	lsdn_names_free(&net->virt_names);
	free(net);
}

void lsdn_net_free(struct lsdn_net *net)
{
	lsdn_foreach(net->virt_list, virt_entry, struct lsdn_virt, v) {
		lsdn_virt_free(v);
	}
	lsdn_foreach(net->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
		phys_detach_by_pa(pa);
	}
	free_helper(net, net_do_free);
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
	phys->state = LSDN_STATE_NEW;
	phys->attr_iface = NULL;
	phys->attr_ip = NULL;
	phys->is_local = false;
	phys->committed_as_local = false;
	lsdn_name_init(&phys->name);
	lsdn_list_init_add(&ctx->phys_list, &phys->phys_entry);
	lsdn_list_init(&phys->attached_to_list);
	ret_ptr(ctx, phys);
}

static void phys_do_free(struct lsdn_phys *phys)
{
	lsdn_list_remove(&phys->phys_entry);
	lsdn_name_free(&phys->name);
	free(phys->attr_iface);
	free(phys->attr_ip);
	free(phys);
}

void lsdn_phys_free(struct lsdn_phys *phys)
{
	lsdn_foreach(phys->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, pa) {
		lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
			lsdn_virt_disconnect(v);
		}
		phys_detach_by_pa(pa);
	}
	free_helper(phys, phys_do_free);
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
	a->state = LSDN_STATE_NEW;

	lsdn_list_init_add(&net->attached_list, &a->attached_entry);
	lsdn_list_init_add(&phys->attached_to_list, &a->attached_to_entry);
	lsdn_list_init(&a->connected_virt_list);
	lsdn_list_init(&a->remote_pa_list);
	lsdn_list_init(&a->pa_view_list);
	a->explicitely_attached = false;
	return a;
}

lsdn_err_t lsdn_phys_attach(struct lsdn_phys *phys, struct lsdn_net* net)
{
	struct lsdn_phys_attachment *a = find_or_create_attachement(phys, net);
	if(!a)
		ret_err(net->ctx, LSDNE_NOMEM);

	if (!a->explicitely_attached)
		renew(&phys->state);

	a->explicitely_attached = true;
	ret_err(net->ctx, LSDNE_OK);
}

static void pa_do_free(struct lsdn_phys_attachment *a)
{
	assert(lsdn_is_list_empty(&a->connected_virt_list));
	assert(!a->explicitely_attached);
	lsdn_list_remove(&a->attached_entry);
	lsdn_list_remove(&a->attached_to_entry);
	free(a);
}

static void free_pa_if_possible(struct lsdn_phys_attachment *a)
{
	/* If not empty, we will wait for the user to remove the virts (or wait for them to be
	 * removed at commit).
	 * Validation will catch the user if he tries to commit a virt connected throught
	 * the phys if the PA is not explicitely attached.
	 */
	if (lsdn_is_list_empty(&a->connected_virt_list) && !a->explicitely_attached) {
		free_helper(a, pa_do_free);
	}
}

static void phys_detach_by_pa(struct lsdn_phys_attachment *a)
{
	a->explicitely_attached = false;
	free_pa_if_possible(a);
}

void lsdn_phys_detach(struct lsdn_phys *phys, struct lsdn_net* net)
{
	lsdn_foreach(phys->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, a) {
		if(a->net == net) {
			phys_detach_by_pa(a);
			return;
		}
	}
}

lsdn_err_t lsdn_phys_set_iface(struct lsdn_phys *phys, const char *iface){
	char* iface_dup = strdup(iface);
	if(iface_dup == NULL)
		ret_err(phys->ctx, LSDNE_NOMEM);

	if (!phys->attr_iface || strcmp(iface, phys->attr_iface))
		renew(&phys->state);

	free(phys->attr_iface);
	phys->attr_iface = iface_dup;
	ret_err(phys->ctx, LSDNE_OK);
}

lsdn_err_t lsdn_phys_clear_iface(struct lsdn_phys *phys){
	free(phys->attr_iface);
	phys->attr_iface = NULL;
	ret_err(phys->ctx, LSDNE_OK);
}

lsdn_err_t lsdn_phys_set_ip(struct lsdn_phys *phys, lsdn_ip_t ip)
{
	lsdn_ip_t *ip_dup = malloc(sizeof(*ip_dup));
	if (ip_dup == NULL)
		ret_err(phys->ctx, LSDNE_NOMEM);
	*ip_dup = ip;

	if (!phys->attr_ip || !lsdn_ip_eq(ip, *phys->attr_ip))
		renew(&phys->state);

	free(phys->attr_ip);
	phys->attr_ip = ip_dup;
	ret_err(phys->ctx, LSDNE_OK);
}

lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys)
{
	if (!phys->is_local) {
		renew(&phys->state);
		phys->is_local = true;
	}

	return LSDNE_OK;
}

lsdn_err_t lsdn_phys_unclaim_local(struct lsdn_phys *phys)
{
	if (phys->is_local) {
		renew(&phys->state);
		phys->is_local = false;
	}
	return LSDNE_OK;
}

struct lsdn_virt *lsdn_virt_new(struct lsdn_net *net){
	struct lsdn_virt *virt = malloc(sizeof(*virt));
	if(!virt)
		ret_ptr(net->ctx, NULL);
	virt->network = net;
	virt->state = LSDN_STATE_NEW;
	virt->attr_mac = NULL;
	virt->connected_through = NULL;
	virt->committed_to = NULL;
	virt->ht_in_rules = NULL;
	virt->ht_out_rules = NULL;
	lsdn_if_init(&virt->connected_if);
	lsdn_if_init(&virt->committed_if);
	lsdn_list_init_add(&net->virt_list, &virt->virt_entry);
	lsdn_list_init(&virt->virt_view_list);
	lsdn_name_init(&virt->name);
	ret_ptr(net->ctx, virt);
}

static void virt_do_free(struct lsdn_virt *virt)
{
	lsdn_vr_do_free_all_rules(virt);
	if (virt->connected_through) {
		lsdn_list_remove(&virt->connected_virt_entry);
		free_pa_if_possible(virt->connected_through);
		virt->connected_through = NULL;
	}
	lsdn_list_remove(&virt->virt_entry);
	lsdn_name_free(&virt->name);
	lsdn_if_free(&virt->connected_if);
	lsdn_if_free(&virt->committed_if);
	free(virt->attr_mac);
	free(virt);
}

void lsdn_virt_free(struct lsdn_virt *virt)
{
	lsdn_vrs_free_all(virt);
	free_helper(virt, virt_do_free);
}

lsdn_err_t lsdn_virt_set_name(struct lsdn_virt *virt, const char *name)
{
	ret_err(virt->network->ctx, lsdn_name_set(&virt->name, &virt->network->virt_names, name));
}

const char* lsdn_virt_get_name(struct lsdn_virt *virt)
{
	return virt->name.str;
}
struct lsdn_virt* lsdn_virt_by_name(struct lsdn_net *net, const char *name)
{
	struct lsdn_name *r = lsdn_names_search(&net->virt_names, name);
	if (!r)
		return NULL;
	return lsdn_container_of(r, struct lsdn_virt, name);
}

lsdn_err_t lsdn_virt_connect(
	struct lsdn_virt *virt, struct lsdn_phys *phys, const char *iface)
{
	struct lsdn_phys_attachment *a = find_or_create_attachement(phys, virt->network);
	if(!a)
		ret_err(phys->ctx, LSDNE_NOMEM);

	lsdn_err_t err = lsdn_if_set_name(&virt->connected_if, iface);
	if(err != LSDNE_OK)
		ret_err(phys->ctx, err);

	lsdn_virt_disconnect(virt);
	virt->connected_through = a;
	renew(&virt->state);
	lsdn_list_init_add(&a->connected_virt_list, &virt->connected_virt_entry);

	ret_err(phys->ctx, LSDNE_OK);
}

void lsdn_virt_disconnect(struct lsdn_virt *virt){
	if(!virt->connected_through)
		return;

	lsdn_list_remove(&virt->connected_virt_entry);
	virt->connected_through = NULL;
	renew(&virt->state);
}

lsdn_err_t lsdn_virt_set_mac(struct lsdn_virt *virt, lsdn_mac_t mac)
{
	lsdn_mac_t *mac_dup = malloc(sizeof(*mac_dup));
	if (mac_dup == NULL)
		ret_err(virt->network->ctx, LSDNE_NOMEM);
	*mac_dup = mac;

	free(virt->attr_mac);
	virt->attr_mac = mac_dup;
	ret_err(virt->network->ctx, LSDNE_OK);
}

lsdn_err_t lsdn_virt_get_recommended_mtu(struct lsdn_virt *virt, unsigned int *mtu)
{
	struct lsdn_context *ctx = virt->network->ctx;
	struct lsdn_net_ops *ops = virt->network->settings->ops;
	if (!virt->connected_through)
		ret_err(ctx, LSDNE_NOIF);
	struct lsdn_phys *phys = virt->connected_through->phys;
	struct lsdn_if phys_if = {0};
	lsdn_err_t ret;
	if (phys && phys->attr_iface) {
		phys_if.ifname = phys->attr_iface;
		ret = lsdn_if_resolve(&phys_if);
		if (ret != LSDNE_OK)
			ret_err(ctx, ret);
		ret = lsdn_link_get_mtu(ctx->nlsock, phys_if.ifindex, mtu);
		if (ret != LSDNE_OK)
			ret_err(ctx, ret);
	} else {
		ret_err(ctx, LSDNE_NOIF);
	}
	*mtu -= ops->compute_tunneling_overhead(virt->connected_through);
	ret_err(ctx, LSDNE_OK);
}

static bool should_be_validated(enum lsdn_state state) {
	return state == LSDN_STATE_NEW || state == LSDN_STATE_RENEW;
}

static bool will_be_deleted(enum lsdn_state state)
{
	return state == LSDN_STATE_DELETE;
}

static void report_virts(struct lsdn_phys_attachment *pa)
{
	lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v)
	{
		if(!should_be_validated(v->state))
			continue;
		lsdn_problem_report(pa->net->ctx, LSDNP_PHYS_NOT_ATTACHED,
			LSDNS_VIRT, v,
			LSDNS_NET, pa->net,
			LSDNS_PHYS, pa->phys,
			LSDNS_END);
	}
}

static void validate_virts_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v){
		if(!should_be_validated(v->state))
			continue;
		if(pa->explicitely_attached && pa->phys->is_local)
		{
			lsdn_err_t err = lsdn_if_resolve(&v->connected_if);
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

static void validate_rules(struct lsdn_virt *virt, struct vr_prio *ht_prio)
{
	struct vr_prio *prio, *tmp;
	HASH_ITER(hh, ht_prio, prio, tmp) {
		/* First check that matches are compatible */
		struct lsdn_vr *first_rule = NULL;
		lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, r) {
			if (first_rule) {
				bool same =
					(memcmp(first_rule->targets, r->targets, sizeof(r->targets)) == 0)
					&& (memcmp(first_rule->masks, r->masks, sizeof(r->masks)) == 0);
				if (!same) {
					lsdn_problem_report(
						virt->network->ctx, LSDNP_VR_INCOMPATIBLE_MATCH,
						LSDNS_VR, first_rule,
						LSDNS_VR, r,
						LSDNS_VIRT, virt);
					/* do not bother doing more checks */
					return;
				}
			} else {
				first_rule = r;
			}
		}

		/* Optional todo:re use the hash table for commited rules that already exists.
		 * This will reduce memory usage (we can reuse the hh from the rule) and we can
		 * get the running time proportional to changed rules, not all rules. */

		/* Then check for conflicting rules */
		struct lsdn_vr *rule_ht = NULL;
		lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, r) {
			struct lsdn_vr *duplicate;
			lsdn_rule_apply_mask(&r->rule, r->targets, r->masks);
			HASH_FIND(hh, rule_ht, r->rule.matches, sizeof(r->rule.matches), duplicate);
			if (duplicate) {
				lsdn_problem_report(
					virt->network->ctx, LSDNP_VR_DUPLICATE_RULE,
					LSDNS_VR, r,
					LSDNS_VR, duplicate,
					LSDNS_VIRT, virt);
				HASH_CLEAR(hh, rule_ht);
				return;
			}
			HASH_ADD(hh, rule_ht, rule.matches, sizeof(r->rule.matches), r);
		}
		HASH_CLEAR(hh, rule_ht);
	}
}

static void commit_vr(
	struct lsdn_virt *virt, struct vr_prio *prio,
	struct lsdn_vr *vr, enum lsdn_direction dir)
{
	if (prio->commited_count == 0) {
		assert(!prio->commited_prio);
		/* This is not reversed: the egress from the virt is our ingress and vice versa */
		struct lsdn_ruleset *rs = (dir == LSDN_IN ? &virt->rules_out : &virt->rules_in);
		vr->rule.subprio = LSDN_VR_SUBPRIO;
		prio->commited_prio = lsdn_ruleset_define_prio(rs, prio->prio_num);
		if (!prio->commited_prio)
			abort();
		memcpy(prio->commited_prio->targets, vr->targets, sizeof(vr->targets));
		memcpy(prio->commited_prio->masks, vr->masks, sizeof(vr->masks));
	}

	lsdn_err_t err = lsdn_ruleset_add(prio->commited_prio, &vr->rule);
	if (err != LSDNE_OK)
		abort();
	prio->commited_count++;
}

static void commit_rules(struct lsdn_virt *virt, struct vr_prio *ht_prio, enum lsdn_direction dir)
{
	struct vr_prio *prio, *tmp;
	HASH_ITER(hh, ht_prio, prio, tmp) {
		lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, r) {
			if (r->state == LSDN_STATE_NEW)
				commit_vr(virt, prio, r, dir);
			ack_state(&r->state);
		}
	}
}

static void decommit_vr(
	struct lsdn_virt *virt, struct vr_prio *prio,
	struct lsdn_vr *vr, enum lsdn_direction dir)
{
	lsdn_ruleset_remove(&vr->rule);
	prio->commited_count--;
	if (prio->commited_count == 0) {
		lsdn_ruleset_remove_prio(prio->commited_prio);
		prio->commited_prio = NULL;
	}
}

static void decommit_rules(struct lsdn_virt *virt, struct vr_prio *ht_prio, enum lsdn_direction dir)
{
	struct vr_prio *prio, *tmp;
	HASH_ITER(hh, ht_prio, prio, tmp) {
		lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, r) {
			propagate(&virt->state, &r->state);
			if (ack_uncommit(&r->state))
				decommit_vr(virt, prio, r, dir);
		}
	}
}

static void validate_virts_net(struct lsdn_net *net)
{
	lsdn_foreach(net->virt_list, virt_entry, struct lsdn_virt, v1) {
		if (!should_be_validated(v1->state) || !v1->attr_mac)
			continue;
		validate_rules(v1, v1->ht_in_rules);
		validate_rules(v1, v1->ht_out_rules);
		lsdn_foreach(net->virt_list, virt_entry, struct lsdn_virt, v2) {
			if (v1 == v2 || !should_be_validated(v2->state) || !v2->attr_mac)
				continue;
			if (lsdn_mac_eq(*v1->attr_mac, *v2->attr_mac))
				lsdn_problem_report(
					net->ctx, LSDNP_VIRT_DUPATTR,
					LSDNS_ATTR, "mac",
					LSDNS_VIRT, v1,
					LSDNS_VIRT, v2,
					LSDNS_NET, net,
					LSDNS_END);
		}
	}
}

static void cross_validate_networks(struct lsdn_net *net1, struct lsdn_net *net2)
{
	struct lsdn_settings *s1 = net1->settings;
	struct lsdn_settings *s2 = net2->settings;

	if (s1->nettype == s2->nettype && net1->vnet_id == net2->vnet_id)
		lsdn_problem_report(
			s1->ctx, LSDNP_NET_DUPID,
			LSDNS_NET, net1,
			LSDNS_NET, net2,
			LSDNS_NETID, net1->vnet_id,
			LSDNS_END);

	bool check_nettypes = false;
	lsdn_foreach(net1->attached_list, attached_entry, struct lsdn_phys_attachment, pa1) {
		if (!pa1->phys->is_local)
			continue;
		lsdn_foreach(net2->attached_list, attached_entry, struct lsdn_phys_attachment, pa2) {
			if (!pa2->phys->is_local)
				continue;
			check_nettypes = true;
		}
	}

	if (check_nettypes) {
		if (s1->nettype == LSDN_NET_VXLAN && s2->nettype == LSDN_NET_VXLAN) {
			if (s1->switch_type == LSDN_STATIC_E2E && s2->switch_type != LSDN_STATIC_E2E) {
				if (s1->vxlan.port == s2->vxlan.port) {
					lsdn_problem_report(
						s1->ctx, LSDNP_NET_BAD_NETTYPE,
						LSDNS_NET, net1,
						LSDNS_NET, net2,
						LSDNS_END);
				}
			}
		}
	}
}

lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	ctx->problem_cb = cb;
	ctx->problem_cb_user = user;
	ctx->problem_count = 0;

	/********* Propagate states (some will be propagated later) *********/
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p) {
		lsdn_foreach(p->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, pa) {
			propagate(&p->state, &pa->state);
		}
	}
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, n){
		lsdn_foreach(n->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
			propagate(&n->state, &pa->state);
		}
	}
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, n){
		lsdn_foreach(n->virt_list, virt_entry, struct lsdn_virt, v) {
			/* Does not matter if we use committed_through or connected_through, if they
			 * have changed, the virt must be renewed anyway */
			if (v->connected_through)
				propagate(&v->connected_through->state, &v->state);
		}
	}

	/******* Do the validation ********/
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, net1) {
		if (will_be_deleted(net1->state))
			continue;
		validate_virts_net(net1);
		lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, net2) {
			if (net1 != net2 && !will_be_deleted(net2->state))
				cross_validate_networks(net1, net2);
		}
	}

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if (will_be_deleted(p->state))
			continue;
		lsdn_foreach(p->attached_to_list, attached_to_entry, struct lsdn_phys_attachment, a)
		{
			if(!a->explicitely_attached){
				report_virts(a);
			}else{
				if (p->is_local && !p->attr_iface)
					lsdn_problem_report(
						ctx, LSDNP_PHYS_NOATTR,
						LSDNS_ATTR, "iface",
						LSDNS_PHYS, p,
						LSDNS_NET, a->net,
						LSDNS_END);

				if(should_be_validated(a->state) && a->net->settings->ops->validate_pa)
					a->net->settings->ops->validate_pa(a);
				validate_virts_pa(a);
			}
		}
		lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p_other) {
			if (p == p_other || will_be_deleted(p_other->state))
				continue;
			if (p->attr_ip && p_other->attr_ip)
				if (lsdn_ip_eq(*p->attr_ip, *p_other->attr_ip))
					lsdn_problem_report(
						ctx, LSDNP_PHYS_DUPATTR,
						LSDNS_ATTR, "ip",
						LSDNS_PHYS, p,
						LSDNS_PHYS, p_other,
						LSDNS_END);
		}
	}

	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, n) {
		lsdn_foreach(
			n->attached_list, attached_entry,
			struct lsdn_phys_attachment, a) {
			if (!a->phys->attr_ip || will_be_deleted(a->phys->state))
				continue;
			lsdn_foreach(
				n->attached_list, attached_entry,
				struct lsdn_phys_attachment, a_other) {
				if (a == a_other)
					continue;
				if (!a_other->phys->attr_ip || will_be_deleted(a_other->phys->state))
					continue;
				if (!lsdn_ipv_eq(*a->phys->attr_ip, *a_other->phys->attr_ip))
					lsdn_problem_report(
						ctx, LSDNP_PHYS_INCOMPATIBLE_IPV,
						LSDNS_PHYS, a->phys,
						LSDNS_PHYS, a_other->phys,
						LSDNS_NET, n,
						LSDNS_END);
			}
		}
	}

	return (ctx->problem_count == 0) ? LSDNE_OK : LSDNE_VALIDATE;
}

static void commit_pa(struct lsdn_phys_attachment *pa, lsdn_problem_cb cb, void *user)
{
	LSDN_UNUSED(cb); LSDN_UNUSED(user);
	struct lsdn_net_ops *ops = pa->net->settings->ops;
	if (pa->state == LSDN_STATE_NEW) {
		lsdn_log(LSDNL_NETOPS, "create_pa(net = %s (%p), phys = %s (%p), pa = %p)\n",
			 lsdn_nullable(pa->net->name.str), pa->net,
			 lsdn_nullable(pa->phys->name.str), pa->phys,
			 pa);
		ops->create_pa(pa);
	}

	lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		if (v->state == LSDN_STATE_NEW) {
			v->committed_to = pa;
			if (lsdn_if_copy(&v->committed_if, &v->connected_if) != LSDNE_OK)
				abort();

			if (ops->add_virt) {
				lsdn_log(LSDNL_NETOPS, "add_virt(net = %s (%p), phys = %s (%p), pa = %p, virt = %s (%p)\n",
					 lsdn_nullable(pa->net->name.str), pa->net,
					 lsdn_nullable(pa->phys->name.str), pa->phys,
					 pa,
					 v->connected_if.ifname, v);
				ops->add_virt(v);
			}
		}
		commit_rules(v, v->ht_in_rules, LSDN_IN);
		commit_rules(v, v->ht_out_rules, LSDN_OUT);
	}

	lsdn_foreach(pa->net->attached_list, attached_entry, struct lsdn_phys_attachment, remote) {
		if (remote == pa)
			continue;
		if (pa->state != LSDN_STATE_NEW && remote->state != LSDN_STATE_NEW)
			continue;

		struct lsdn_remote_pa *rpa = malloc(sizeof(*rpa));
		if (!rpa)
			abort();
		rpa->local = pa;
		rpa->remote = remote;
		lsdn_list_init_add(&remote->pa_view_list, &rpa->pa_view_entry);
		lsdn_list_init_add(&pa->remote_pa_list, &rpa->remote_pa_entry);
		lsdn_list_init(&rpa->remote_virt_list);
		if (ops->add_remote_pa) {
			lsdn_log(LSDNL_NETOPS, "add_remote_pa("
				 "net = %s (%p), local_phys = %s (%p), remote_phys = %s (%p), "
				 "local_pa = %p, remote_pa = %p, remote_pa_view = %p)\n",
				 lsdn_nullable(pa->net->name.str), pa->net,
				 lsdn_nullable(pa->phys->name.str), pa->phys,
				 lsdn_nullable(remote->phys->name.str), remote->phys,
				 pa, remote, rpa);
			ops->add_remote_pa(rpa);
		}
	}

	lsdn_foreach(pa->remote_pa_list, remote_pa_entry, struct lsdn_remote_pa, remote) {
		lsdn_foreach(remote->remote->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
			if (pa->state != LSDN_STATE_NEW && v->state != LSDN_STATE_NEW)
				continue;
			struct lsdn_remote_virt *rvirt = malloc(sizeof(*rvirt));
			if(!rvirt)
				abort();
			rvirt->pa = remote;
			rvirt->virt = v;
			lsdn_list_init_add(&v->virt_view_list, &rvirt->virt_view_entry);
			lsdn_list_init_add(&remote->remote_virt_list, &rvirt->remote_virt_entry);
			if (ops->add_remote_virt) {
				lsdn_log(LSDNL_NETOPS, "add_remote_virt("
					 "net = %s (%p), local_phys = %s (%p), remote_phys = %s (%p), "
					 "local_pa = %p, remote_pa = %p, remote_pa_view = %p, virt = %p)\n",
					 lsdn_nullable(pa->net->name.str), pa->net,
					 lsdn_nullable(pa->phys->name.str), pa->phys,
					 lsdn_nullable(remote->remote->phys->name.str), remote->remote->phys,
					 pa, remote->remote, remote, v);
				ops->add_remote_virt(rvirt);
			}
		}
	}
}

static void decommit_remote_virt(struct lsdn_remote_virt *rv)
{
	struct lsdn_net_ops *ops = rv->virt->network->settings->ops;
	if (ops->remove_remote_virt) {
		lsdn_log(LSDNL_NETOPS, "remove_remote_virt("
				"net = %s (%p), local_phys = %s (%p), remote_phys = %s (%p), "
				"local_pa = %p, remote_pa = %p, remote_pa_view = %p, virt = %p)\n",
				lsdn_nullable(rv->virt->network->name.str), rv->virt->network,
				lsdn_nullable(rv->pa->local->phys->name.str), rv->pa->local->phys,
				lsdn_nullable(rv->pa->remote->phys->name.str), rv->pa->remote->phys,
				rv->pa->local, rv->pa->local, rv->pa, rv->virt);
		ops->remove_remote_virt(rv);
	}
	lsdn_list_remove(&rv->remote_virt_entry);
	lsdn_list_remove(&rv->virt_view_entry);
	free(rv);
}

static void decommit_virt(struct lsdn_virt *v)
{
	struct lsdn_net_ops *ops = v->network->settings->ops;
	struct lsdn_phys_attachment *pa = v->committed_to;

	lsdn_foreach(v->virt_view_list, virt_view_entry, struct lsdn_remote_virt, rv) {
		decommit_remote_virt(rv);
	}

	if (pa) {
		if (ops->remove_virt) {
			lsdn_log(LSDNL_NETOPS, "remove_virt(net = %s (%p), phys = %s (%p), pa = %p, virt = %s (%p)\n",
				 lsdn_nullable(pa->net->name.str), pa->net,
				 lsdn_nullable(pa->phys->name.str), pa->phys,
				 pa,
				 v->committed_if.ifname, v);
			ops->remove_virt(v);
		}
		v->committed_to = NULL;
		lsdn_if_reset(&v->committed_if);
	}
}

static void decommit_remote_pa(struct lsdn_remote_pa *rpa)
{
	struct lsdn_phys_attachment *local = rpa->local;
	struct lsdn_phys_attachment *remote = rpa->remote;
	struct lsdn_net_ops *ops = local->net->settings->ops;

	lsdn_foreach(rpa->remote_virt_list, remote_virt_entry, struct lsdn_remote_virt, rv) {
		decommit_remote_virt(rv);
	}

	if (ops->remove_remote_pa) {
		lsdn_log(LSDNL_NETOPS, "remove_remote_pa("
			 "net = %s (%p), local_phys = %s (%p), remote_phys = %s (%p), "
			 "local_pa = %p, remote_pa = %p, remote_pa_view = %p)\n",
			 lsdn_nullable(local->net->name.str), local->net,
			 lsdn_nullable(local->phys->name.str), local->phys,
			 lsdn_nullable(remote->phys->name.str), remote->phys,
			 local, remote, rpa);
		ops->remove_remote_pa(rpa);
	}
	lsdn_list_remove(&rpa->pa_view_entry);
	lsdn_list_remove(&rpa->remote_pa_entry);
	assert(lsdn_is_list_empty(&rpa->remote_virt_list));
	free(rpa);
}

static void decommit_pa(struct lsdn_phys_attachment *pa)
{
	struct lsdn_net_ops *ops = pa->net->settings->ops;

	lsdn_foreach(pa->pa_view_list, pa_view_entry, struct lsdn_remote_pa, rpa) {
		decommit_remote_pa(rpa);
	}

	lsdn_foreach(pa->remote_pa_list, remote_pa_entry, struct lsdn_remote_pa, rpa) {
		decommit_remote_pa(rpa);
	}

	if (pa->phys->committed_as_local) {
		if (ops->destroy_pa) {
			lsdn_log(LSDNL_NETOPS, "destroy_pa(net = %s (%p), phys = %s (%p), pa = %p)\n",
				 lsdn_nullable(pa->net->name.str), pa->net,
				 lsdn_nullable(pa->phys->name.str), pa->phys,
				 pa);
			ops->destroy_pa(pa);
		}
	}
}

static void trigger_startup_hooks(struct lsdn_context *ctx)
{
	// TODO: only do for new PAs
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

	/* List of objects to process:
	 *	setting, network, phys, physical attachment, virt
	 * Settings, networks and attachments do not need to be committed in any way, but we must keep them
	 * alive until PAs and virts are deleted. */

	/********* Decommit phase **********/
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, n) {
		lsdn_foreach(n->virt_list, virt_entry, struct lsdn_virt, v) {
			decommit_rules(v, v->ht_in_rules, LSDN_IN);
			decommit_rules(v, v->ht_out_rules, LSDN_OUT);
			if (ack_uncommit(&v->state)) {
				decommit_virt(v);
				ack_delete(v, virt_do_free);
			}
		}
		lsdn_foreach(n->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
			if (ack_uncommit(&pa->state)) {
				decommit_pa(pa);
				ack_delete(pa, pa_do_free);
			}
		}
		if (ack_uncommit(&n->state))
			ack_delete(n, net_do_free);
	}

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if (ack_uncommit(&p->state))
			ack_delete(p, phys_do_free)
	}

	lsdn_foreach(ctx->settings_list, settings_entry, struct lsdn_settings, s) {
		if (ack_uncommit(&s->state))
			ack_delete(s, settings_do_free);
	}

	/********* (Re)commit phase **********/
	/* first create physical attachments for local physes and populate
	 * them with virts, remote PAs and remote virts */
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if (p->is_local) {
			p->committed_as_local = p->is_local;
			lsdn_foreach(
				p->attached_to_list, attached_to_entry,
				struct lsdn_phys_attachment, pa)
			{
				commit_pa(pa, cb, user);
			}
		}
	}

	/********* Ack phase **********/
	lsdn_foreach(ctx->settings_list, settings_entry, struct lsdn_settings, s) {
		ack_state(&s->state);
	}

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		ack_state(&p->state);
	}

	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, n){
		ack_state(&n->state);
		lsdn_foreach(n->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
			ack_state(&pa->state);
		}
		lsdn_foreach(n->virt_list, virt_entry, struct lsdn_virt, v) {
			ack_state(&v->state);
		}
	}

	return (ctx->problem_count == 0) ? LSDNE_OK : LSDNE_COMMIT;
}
