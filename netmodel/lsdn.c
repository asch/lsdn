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

/** Generate unique name for an object.
 * @ingroup misc
 * The name is based on the context name, type of the object (net, phys, virt, etc.) and
 * a unique object counter on the context. It is in the form `"ctxname-type-12"`.
 *
 * Results are saved in `lsdn_context.namebuf`, so every subsequent call overwrites the previous
 * results. Users need to make a copy of the returned string. This is not a problem because
 * the usual usage is inside `lsdn_name_set` which does make a private copy.
 *
 * @see lsdn_mk_net_name, lsdn_mk_phys_name, lsdn_mk_virt_name, lsdn_mk_iface_name, lsdn_mk_settings_name
 * @param ctx LSDN context
 * @param type object type (arbitrary string, usually "net", "phys", "virt", "iface" or "settings")
 * @return pointer to a buffer with a generated unique name. */
const char *lsdn_mk_name(struct lsdn_context *ctx, const char *type)
{
	snprintf(ctx->namebuf, sizeof(ctx->namebuf), "%s-%s-%d", ctx->name, type, ++ctx->obj_count);
	return ctx->namebuf;
}

/** Create new LSDN context.
 * Initialize a #lsdn_context struct and set its name to `name`.
 * The returned struct must be freed by #lsdn_context_free or #lsdn_context_cleanup after use.
 * @param name Context name.
 * @return `NULL` if allocation failed, pointer to new #lsdn_context otherwise. */
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

	ctx->nlsock = NULL;
	ctx->obj_count = 0;
	lsdn_names_init(&ctx->phys_names);
	lsdn_names_init(&ctx->net_names);
	lsdn_names_init(&ctx->setting_names);
	lsdn_list_init(&ctx->networks_list);
	lsdn_list_init(&ctx->settings_list);
	lsdn_list_init(&ctx->phys_list);
	return ctx;
}

static lsdn_err_t lsdn_context_ensure_socket(struct lsdn_context *ctx)
{
	if (ctx->nlsock)
		return LSDNE_OK;
	ctx->nlsock = lsdn_socket_init();
	if(!ctx->nlsock)
		return LSDNE_NETLINK;

	return LSDNE_OK;
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
 * This sets the out-of-memory callback to a predefined function that prints
 * an error to `stderr` and aborts the program.
 *
 * It is recommended to use this, unless you have a specific way to handle
 * out-of-memory conditions.
 * @see lsdn_abort_cb
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

/** Create a new network.
 * Creates a virtual network object with id `vnet_id`, configured by `s`.
 *
 * Multiple networks can share the same `lsdn_settings`, as long as they differ
 * by `vnet_id`.
 *
 * @return newly allocated `lsdn_net` structure. */
struct lsdn_net *lsdn_net_new(struct lsdn_settings *s, uint32_t vnet_id)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		ret_ptr(s->ctx, NULL);

	net->ctx = s->ctx;
	net->state = LSDN_STATE_NEW;
	net->pending_free = false;
	net->settings = s;
	net->vnet_id = vnet_id;

	lsdn_name_init(&net->name);
	lsdn_names_init(&net->virt_names);
	lsdn_err_t err = lsdn_name_set(&net->name, &net->ctx->net_names, lsdn_mk_net_name(net->ctx));
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(net);
		ret_ptr(s->ctx, NULL);
	}
	lsdn_list_init_add(&s->setting_users_list, &net->settings_users_entry);
	lsdn_list_init_add(&s->ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	ret_ptr(s->ctx, net);
}

/** Perform freeing of a network object.
 * Used when `lsdn_net_free` is manually invoked, as the last step,
 * and also implicitly as part of the decommit phase. */
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

/** Free a network.
 * Ensures that all virts in the network are freed and all physes detached. */
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

/** Set a name for the network. */
lsdn_err_t lsdn_net_set_name(struct lsdn_net *net, const char *name)
{
	ret_err(net->ctx, lsdn_name_set(&net->name, &net->ctx->net_names, name));
}

/** Get the network's name. */
const char* lsdn_net_get_name(struct lsdn_net *net)
{
	return net->name.str;
}

/** Find a network by name.
 * @return `lsdn_net` structure if a network with this name exists.
 * @return `NULL` otherwise. */
struct lsdn_net* lsdn_net_by_name(struct lsdn_context *ctx, const char *name)
{
	struct lsdn_name *r = lsdn_names_search(&ctx->net_names, name);
	if(!r)
		return NULL;
	return lsdn_container_of(r, struct lsdn_net, name);
}

/** Create a new phys.
 * Allocates and initializes a `lsdn_phys` structure.
 *
 * @param ctx LSDN context.
 * @return newly allocated `lsdn_phys` structure. */
struct lsdn_phys *lsdn_phys_new(struct lsdn_context *ctx)
{
	struct lsdn_phys *phys = malloc(sizeof(*phys));
	if(!phys)
		ret_ptr(ctx, NULL);

	phys->ctx = ctx;
	phys->state = LSDN_STATE_NEW;
	phys->pending_free = false;
	phys->attr_iface = NULL;
	phys->attr_ip = NULL;
	phys->is_local = false;
	phys->committed_as_local = false;
	lsdn_name_init(&phys->name);
	lsdn_err_t err = lsdn_name_set(&phys->name, &ctx->phys_names, lsdn_mk_phys_name(ctx));
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(phys);
		ret_ptr(ctx, NULL);
	}
	lsdn_list_init_add(&ctx->phys_list, &phys->phys_entry);
	lsdn_list_init(&phys->attached_to_list);
	ret_ptr(ctx, phys);
}

/** Perform freeing of a phys object.
 * Used when `lsdn_phys_free` is manually invoked, as the last step,
 * and also implicitly as part of the decommit phase. */
static void phys_do_free(struct lsdn_phys *phys)
{
	lsdn_list_remove(&phys->phys_entry);
	lsdn_name_free(&phys->name);
	free(phys->attr_iface);
	free(phys->attr_ip);
	free(phys);
}

/** Free a phys.
 * Ensures that all virts on this phys are disconnected first.
 * @param phys Phys. */
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

/** Set a name for phys.
 * @param phys Phys.
 * @param name New name string. Can be `NULL`.
 * @retval LSDNE_OK Name set successfully.
 * @retval LSDNE_DUPLICATE Phys with the same name already exists.
 * @retval LSDNE_NOMEM Failed to allocate memory for name. */
lsdn_err_t lsdn_phys_set_name(struct lsdn_phys *phys, const char *name)
{
	ret_err(phys->ctx, lsdn_name_set(&phys->name, &phys->ctx->phys_names, name));
}

/** Get the phys's name.
 * @param phys Phys.
 * @return pointer to phys's name. */
const char* lsdn_phys_get_name(struct lsdn_phys *phys)
{
	return phys->name.str;
}

/** Find a phys by name.
 * @param ctx LSDN context.
 * @param name Requested name.
 * @return `lsdn_phys` structure if a phys with this name exists. `NULL` otherwise. */
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
	a->pending_free = false;

	lsdn_list_init_add(&net->attached_list, &a->attached_entry);
	lsdn_list_init_add(&phys->attached_to_list, &a->attached_to_entry);
	lsdn_list_init(&a->connected_virt_list);
	lsdn_list_init(&a->remote_pa_list);
	lsdn_list_init(&a->pa_view_list);
	a->explicitly_attached = false;
	return a;
}

/** Attach phys to a virtual network.
 * Marks the phys as a participant in virtual network `net`. This must be done
 * before any virts are allowed to connect to `net` through this phys.
 *
 * You can attach a phys to multiple virtual networks.
 *
 * @param phys Phys.
 * @param net Virtual network.
 *
 * @retval LSDNE_OK Attachment succeeded
 * @retval LSDNE_NOMEM Failed to allocate memory for attachment. */
lsdn_err_t lsdn_phys_attach(struct lsdn_phys *phys, struct lsdn_net* net)
{
	struct lsdn_phys_attachment *a = find_or_create_attachement(phys, net);
	if(!a)
		ret_err(net->ctx, LSDNE_NOMEM);

	if (!a->explicitly_attached)
		renew(&phys->state);

	a->explicitly_attached = true;
	ret_err(net->ctx, LSDNE_OK);
}

static void pa_do_free(struct lsdn_phys_attachment *a)
{
	assert(lsdn_is_list_empty(&a->connected_virt_list));
	assert(!a->explicitly_attached);
	lsdn_list_remove(&a->attached_entry);
	lsdn_list_remove(&a->attached_to_entry);
	free(a);
}

static void free_pa_if_possible(struct lsdn_phys_attachment *a)
{
	/* If not empty, we will wait for the user to remove the virts (or wait for them to be
	 * removed at commit).
	 * Validation will catch the user if he tries to commit a virt connected throught
	 * the phys if the PA is not explicitly attached.
	 */
	if (lsdn_is_list_empty(&a->connected_virt_list) && !a->explicitly_attached) {
		free_helper(a, pa_do_free);
	}
}

static void phys_detach_by_pa(struct lsdn_phys_attachment *a)
{
	a->explicitly_attached = false;
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

/** Assign a local phys.
 * All participants in a LSDN network must share a compatible memory model.
 * That means that every host's model contains all the physes in the network.
 * This function configures a particular phys to be the local machine. Only
 * rules related to virts on the local phys are entered into the kernel tables. */
lsdn_err_t lsdn_phys_claim_local(struct lsdn_phys *phys)
{
	if (!phys->is_local) {
		renew(&phys->state);
		phys->is_local = true;
	}

	return LSDNE_OK;
}

/** Unassign a local phys.
 * @see lsdn_phys_claim_local */
lsdn_err_t lsdn_phys_unclaim_local(struct lsdn_phys *phys)
{
	if (phys->is_local) {
		renew(&phys->state);
		phys->is_local = false;
	}
	return LSDNE_OK;
}

/** Create a new virt.
 * Creates a virt as part of `net`.
 *
 * @return newly allocated `lsdn_virt` structure. */
struct lsdn_virt *lsdn_virt_new(struct lsdn_net *net){
	struct lsdn_virt *virt = malloc(sizeof(*virt));
	if(!virt)
		ret_ptr(net->ctx, NULL);
	virt->network = net;
	virt->state = LSDN_STATE_NEW;
	virt->pending_free = false;
	virt->attr_mac = NULL;
	virt->attr_rate_in = NULL;
	virt->attr_rate_out = NULL;
	virt->connected_through = NULL;
	virt->committed_to = NULL;
	virt->ht_in_rules = NULL;
	virt->ht_out_rules = NULL;
	virt->commited_policing_in = NULL;
	virt->commited_policing_out = NULL;
	lsdn_if_init(&virt->connected_if);
	lsdn_if_init(&virt->committed_if);
	lsdn_name_init(&virt->name);
	lsdn_err_t err = lsdn_name_set(&virt->name, &net->virt_names, lsdn_mk_virt_name(net->ctx));
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(virt);
		ret_ptr(net->ctx, NULL);
	}
	lsdn_list_init_add(&net->virt_list, &virt->virt_entry);
	lsdn_list_init(&virt->virt_view_list);
	ret_ptr(net->ctx, virt);
}

struct lsdn_net *lsdn_virt_get_net(struct lsdn_virt *virt)
{
	return virt->network;
}

/** Perform freeing of a virt object.
 * Used when `lsdn_virt_free` is manually invoked, as the last step,
 * and also implicitly as part of the decommit phase. */
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
	free(virt->attr_rate_in);
	free(virt->attr_rate_out);
	free(virt);
}

/** Free a virt. */
void lsdn_virt_free(struct lsdn_virt *virt)
{
	lsdn_vrs_free_all(virt);
	free_helper(virt, virt_do_free);
}

/** Set a name for the virt. */
lsdn_err_t lsdn_virt_set_name(struct lsdn_virt *virt, const char *name)
{
	ret_err(virt->network->ctx, lsdn_name_set(&virt->name, &virt->network->virt_names, name));
}

/** Get the virt's name. */
const char* lsdn_virt_get_name(struct lsdn_virt *virt)
{
	return virt->name.str;
}

/** Find a virt by name.
 * @return `lsdn_virt` structure if a network with this name exists.
 * @return `NULL` otherwise. */
struct lsdn_virt* lsdn_virt_by_name(struct lsdn_net *net, const char *name)
{
	struct lsdn_name *r = lsdn_names_search(&net->virt_names, name);
	if (!r)
		return NULL;
	return lsdn_container_of(r, struct lsdn_virt, name);
}

/** Connect a virt to its network.
 * Associates a virt with a given phys and a network interface, and ensures that
 * this interface will receive the network's traffic.
 * @param virt virt to connect.
 * @param phys phys on which the virt exists.
 * @param iface name of Linux network interface on the given phys, which will
 * receive the virt's traffic.
 * @see \ref lsdn-public */
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

/** Disconnects a virt from its network.
 * Disconnected virt will no longer be able to send and receive traffic. */
void lsdn_virt_disconnect(struct lsdn_virt *virt){
	if(!virt->connected_through)
		return;

	lsdn_list_remove(&virt->connected_virt_entry);
	virt->connected_through = NULL;
	renew(&virt->state);
}

/** Set virt's MAC address. */
lsdn_err_t lsdn_virt_set_mac(struct lsdn_virt *virt, lsdn_mac_t mac)
{
	lsdn_mac_t *mac_dup = malloc(sizeof(*mac_dup));
	if (mac_dup == NULL)
		ret_err(virt->network->ctx, LSDNE_NOMEM);
	*mac_dup = mac;

	free(virt->attr_mac);
	virt->attr_mac = mac_dup;
	renew(&virt->state);
	ret_err(virt->network->ctx, LSDNE_OK);
}

/** Get recommended MTU for a given virt.
 * Calculates the appropriate MTU value, taking into account the network's tunneling 
 * method overhead. */
lsdn_err_t lsdn_virt_get_recommended_mtu(struct lsdn_virt *virt, unsigned int *mtu)
{	
	struct lsdn_context *ctx = virt->network->ctx;
	struct lsdn_net_ops *ops = virt->network->settings->ops;
	if (!virt->connected_through)
		ret_err(ctx, LSDNE_NOIF);
	struct lsdn_phys *phys = virt->connected_through->phys;
	struct lsdn_if phys_if = {0};
	lsdn_err_t ret = lsdn_context_ensure_socket(virt->network->ctx);
	if (ret != LSDNE_OK)
		ret_err(ctx, ret);

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

lsdn_err_t lsdn_virt_set_rate_in(struct lsdn_virt *virt, lsdn_qos_rate_t rate)
{
	lsdn_qos_rate_t *rate_dup = malloc(sizeof(*rate_dup));
	if (rate_dup == NULL)
		ret_err(virt->network->ctx, LSDNE_NOMEM);
	*rate_dup = rate;
	free(virt->attr_rate_in);
	virt->attr_rate_in = rate_dup;
	renew(&virt->state);
	ret_err(virt->network->ctx, LSDNE_OK);
}

lsdn_err_t lsdn_virt_set_rate_out(struct lsdn_virt *virt, lsdn_qos_rate_t rate)
{
	lsdn_qos_rate_t *rate_dup = malloc(sizeof(*rate_dup));
	if (rate_dup == NULL)
		ret_err(virt->network->ctx, LSDNE_NOMEM);
	*rate_dup = rate;
	free(virt->attr_rate_out);
	virt->attr_rate_out = rate_dup;
	renew(&virt->state);
	ret_err(virt->network->ctx, LSDNE_OK);
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
		if(pa->explicitly_attached && pa->phys->is_local)
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
						LSDNS_VIRT, virt,
						LSDNS_END);
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
					LSDNS_VIRT, virt,
					LSDNS_END);
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
			if (ack_decommit(&r->state))
				decommit_vr(virt, prio, r, dir);
		}
	}
}

static void rates_action(struct lsdn_filter *f, uint16_t order, struct lsdn_virt *v, const lsdn_qos_rate_t *rate)
{
	unsigned int mtu = 0xFFFF;
	(void) lsdn_virt_get_recommended_mtu(v, &mtu);
	lsdn_action_police(f,
		order, rate->avg_rate, rate->burst_size, rate->burst_rate, mtu,
		TC_ACT_PIPE, TC_ACT_SHOT);
	lsdn_action_continue(f, order+1);
}

static void rates_action_out(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_virt *v = user;
	const lsdn_qos_rate_t *rate = v->attr_rate_out;
	rates_action(f, order, v, rate);
}

static void rates_action_in(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_virt *v = user;
	const lsdn_qos_rate_t *rate = v->attr_rate_in;
	rates_action(f, order, v, rate);
}

static lsdn_err_t commit_rates_inout(
	struct lsdn_virt *v,
	struct lsdn_ruleset_prio **commited_policing_prio,
	struct lsdn_rule *comitted_policing_rule,
	struct lsdn_ruleset *rules,
	lsdn_mkaction_fn action)
{
	lsdn_err_t err;

	*commited_policing_prio =
		lsdn_ruleset_define_prio(rules, LSDN_IF_PRIO_POLICING);
	if (!*commited_policing_prio)
		return LSDNE_NOMEM;
	comitted_policing_rule->subprio = 0;
	lsdn_action_init(&comitted_policing_rule->action, 2, action, v);
	err = lsdn_ruleset_add(*commited_policing_prio, comitted_policing_rule);
	if (err != LSDNE_OK) {
		err = lsdn_ruleset_remove_prio(*commited_policing_prio);
		if (err != LSDNE_OK) {
			v->network->ctx->inconsistent = true;
			return LSDNE_INCONSISTENT;
		}
		*commited_policing_prio = NULL;
	}
	return LSDNE_OK;
}

static void decommit_rates(struct lsdn_virt *virt);
static lsdn_err_t commit_rates(struct lsdn_virt *virt)
{
	/* If you think the ins/outs are reversed here, please see comments at virt->rules_out and
	 * virt->attr_rate_out.*/
	lsdn_err_t err;
	if (virt->attr_rate_in) {
		err = commit_rates_inout(
			virt, &virt->commited_policing_in, &virt->commited_policing_rule_in,
			&virt->rules_out, rates_action_in);
		if (err != LSDNE_OK)
			return err;
	}


	if (virt->attr_rate_out) {
		err = commit_rates_inout(
			virt, &virt->commited_policing_out, &virt->commited_policing_rule_out,
			&virt->rules_in, rates_action_out);
		if (err != LSDNE_OK) {
			decommit_rates(virt);
			return err;
		}
	}
	return LSDNE_OK;
}

static void decommit_rates(struct lsdn_virt *virt)
{
	struct lsdn_context *ctx = virt->network->ctx;
	if (virt->commited_policing_in) {
		mark_commit_err(ctx, &virt->state, LSDNS_VIRT, virt, true,
			lsdn_ruleset_remove(&virt->commited_policing_rule_in));
		mark_commit_err(ctx, &virt->state, LSDNS_VIRT, virt, true,
			lsdn_ruleset_remove_prio(virt->commited_policing_in));
		virt->commited_policing_in = NULL;
	}
	if (virt->commited_policing_out) {
		mark_commit_err(ctx, &virt->state, LSDNS_VIRT, virt, true,
			lsdn_ruleset_remove(&virt->commited_policing_rule_out));
		mark_commit_err(ctx, &virt->state, LSDNS_VIRT, virt, true,
			lsdn_ruleset_remove_prio(virt->commited_policing_out));
		virt->commited_policing_out = NULL;
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

/** Validate network model.
 * Walks the currently configured in-memory network model and checks for problems.
 * If problems are found, an error code is returned. Problem callback is also invoked
 * for every problem encountered.
 *
 * @param ctx LSDN context.
 * @param cb Problem callback.
 * @param user User data for the problem callback.
 * @retval #LSDNE_OK No problems detected.
 * @retval #LSDNE_VALIDATE Some problems detected. */
lsdn_err_t lsdn_validate(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	ctx->problem_cb = cb;
	ctx->problem_cb_user = user;
	ctx->problem_count = 0;
	ctx->inconsistent = false;

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
			if(!a->explicitly_attached){
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

static void decommit_pa(struct lsdn_phys_attachment *pa);
static void decommit_virt(struct lsdn_virt *v);

static void commit_pa(struct lsdn_phys_attachment *pa)
{
	struct lsdn_net_ops *ops = pa->net->settings->ops;
	struct lsdn_context *ctx = pa->net->ctx;
	if (pa->state == LSDN_STATE_NEW) {
		lsdn_log(LSDNL_NETOPS, "create_pa(net = %s (%p), phys = %s (%p), pa = %p)\n",
			 lsdn_nullable(pa->net->name.str), pa->net,
			 lsdn_nullable(pa->phys->name.str), pa->phys,
			 pa);
		mark_commit_err(ctx, &pa->state, LSDNS_PA, pa, false,
			ops->create_pa(pa));
	}

	if (!state_ok(pa->state))
		return;

	lsdn_foreach(pa->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
		if (v->state == LSDN_STATE_NEW) {
			struct lsdn_if if2;
			struct lsdn_phys_attachment *old_commited_to = v->committed_to;
			lsdn_if_init(&if2);
			if (lsdn_if_copy(&if2, &v->connected_if) != LSDNE_OK) {
				v->state = LSDN_STATE_ERR;
				lsdn_problem_report(ctx, LSDNP_COMMIT_NOMEM, LSDNS_VIRT, v, LSDNS_END);
				continue;
			}
			v->committed_to = pa;
			lsdn_if_swap(&if2, &v->committed_if);

			if (ops->add_virt) {
				lsdn_log(LSDNL_NETOPS, "add_virt(net = %s (%p), phys = %s (%p), pa = %p, virt = %s (%p)\n",
					 lsdn_nullable(pa->net->name.str), pa->net,
					 lsdn_nullable(pa->phys->name.str), pa->phys,
					 pa,
					 v->connected_if.ifname, v);
				if (mark_commit_err(ctx, &v->state, LSDNS_VIRT, v, false,
					ops->add_virt(v))) {
					// roll back commitment properties
					lsdn_if_swap(&if2, &v->committed_if);
					lsdn_if_free(&if2);
					v->committed_to = old_commited_to;
					continue;
				}
			}

			if (mark_commit_err(ctx, &v->state, LSDNS_VIRT, v, false,
				commit_rates(v))) {
				if (v->state == LSDN_STATE_ERR) {
					// roll back everything
					if (ops->remove_virt(v) != LSDNE_OK) {
						v->state = LSDN_STATE_FAIL;
						v->network->ctx->inconsistent = true;
						continue;
					}
					lsdn_if_swap(&if2, &v->committed_if);
					lsdn_if_free(&if2);
					v->committed_to = old_commited_to;
				}
				continue;
			}
			lsdn_if_free(&if2);
		}
		if (state_ok(v->state)) {
			commit_rules(v, v->ht_in_rules, LSDN_IN);
			commit_rules(v, v->ht_out_rules, LSDN_OUT);
		}
	}

	lsdn_foreach(pa->net->attached_list, attached_entry, struct lsdn_phys_attachment, remote) {
		if (remote == pa)
			continue;
		if (pa->state != LSDN_STATE_NEW && remote->state != LSDN_STATE_NEW)
			continue;

		struct lsdn_remote_pa *rpa = malloc(sizeof(*rpa));
		if (!rpa) {
			lsdn_problem_report(ctx, LSDNP_COMMIT_NOMEM, LSDNS_PA, pa, LSDNS_END);
			pa->state = LSDN_STATE_ERR;
			decommit_pa(remote);
			continue;
		}

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
			if (mark_commit_err(ctx, &remote->state, LSDNS_PA, remote, false,
				ops->add_remote_pa(rpa)))
			{
				lsdn_list_remove(&rpa->pa_view_entry);
				lsdn_list_remove(&rpa->remote_pa_entry);
				free(rpa);
				decommit_pa(remote);
				continue;
			}
		}
	}

	lsdn_foreach(pa->remote_pa_list, remote_pa_entry, struct lsdn_remote_pa, remote) {
		lsdn_foreach(remote->remote->connected_virt_list, connected_virt_entry, struct lsdn_virt, v) {
			if (pa->state != LSDN_STATE_NEW && v->state != LSDN_STATE_NEW)
				continue;
			struct lsdn_remote_virt *rvirt = malloc(sizeof(*rvirt));
			if(!rvirt) {
				lsdn_problem_report(ctx, LSDNP_COMMIT_NOMEM, LSDNS_VIRT, v, LSDNS_END);
				pa->state = LSDN_STATE_ERR;
				decommit_virt(v);
				continue;
			}
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
				if (mark_commit_err(ctx, &v->state, LSDNS_VIRT,v, false,
					ops->add_remote_virt(rvirt)))
				{
					decommit_virt(v);
				}
			}
		}
	}
}

static void decommit_remote_virt(struct lsdn_remote_virt *rv)
{
	struct lsdn_net_ops *ops = rv->virt->network->settings->ops;
	struct lsdn_context *ctx = rv->pa->local->net->ctx;
	if (ops->remove_remote_virt) {
		lsdn_log(LSDNL_NETOPS, "remove_remote_virt("
				"net = %s (%p), local_phys = %s (%p), remote_phys = %s (%p), "
				"local_pa = %p, remote_pa = %p, remote_pa_view = %p, virt = %p)\n",
				lsdn_nullable(rv->virt->network->name.str), rv->virt->network,
				lsdn_nullable(rv->pa->local->phys->name.str), rv->pa->local->phys,
				lsdn_nullable(rv->pa->remote->phys->name.str), rv->pa->remote->phys,
				rv->pa->local, rv->pa->local, rv->pa, rv->virt);
		mark_commit_err(ctx, &rv->virt->state, LSDNS_VIRT, rv->virt, true,
				ops->remove_remote_virt(rv));
	}
	lsdn_list_remove(&rv->remote_virt_entry);
	lsdn_list_remove(&rv->virt_view_entry);
	free(rv);
}

static void decommit_virt(struct lsdn_virt *v)
{
	struct lsdn_net_ops *ops = v->network->settings->ops;
	struct lsdn_phys_attachment *pa = v->committed_to;

	decommit_rates(v);
	decommit_rules(v, v->ht_in_rules, LSDN_IN);
	decommit_rules(v, v->ht_out_rules, LSDN_OUT);

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
			mark_commit_err(v->network->ctx, &v->state, LSDNS_VIRT, v, true, ops->remove_virt(v));
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
	struct lsdn_context *ctx = local->net->ctx;

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
		mark_commit_err(ctx, &remote->state, LSDNS_PA, remote, true, ops->remove_remote_pa(rpa));
	}
	lsdn_list_remove(&rpa->pa_view_entry);
	lsdn_list_remove(&rpa->remote_pa_entry);
	assert(lsdn_is_list_empty(&rpa->remote_virt_list));
	free(rpa);
}

static void decommit_pa(struct lsdn_phys_attachment *pa)
{
	struct lsdn_context *ctx = pa->net->ctx;
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
			mark_commit_err(ctx, &pa->state, LSDNS_PA, pa, true, ops->destroy_pa(pa));
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

/** Commit network model to kernel tables.
 * Calculates tc rules based on the current network model, and its difference from
 * the previously committed network model, and applies the changes. After returning successfully,
 * the current network model is in effect.
 *
 * Performs a model validation (equivalent to calling #lsdn_validate) and returns an error
 * if it fails. Afterwards, works through the memory model in two phases:
 * * In _decommit_ phase, rules belonging to modified (or deleted) objects are removed from kernel tables.
 *   Deleted objects are also freed from memory.
 * * In _recommit_ phase, new rules are installed that correspond to new objects -- or new properties
 *   of objects that were removed in the previous phase.
 *
 * If an error occurs in the recommit phase, a limited rollback is performed and the kernel rules remain
 * in mixed state. Some objects may have been successfully committed, others might still be in the old state
 * because the commit failed. In such case, #LSDNE_COMMIT is returned and the user can retry the commit, to
 * install the remaining objects.
 *
 * If an error occurs in the decommit phase, however, there is no safe way to recover. Given that kernel rules
 * are not installed atomically and there are usually several rules tied to an object, LSDN can't know what is
 * the installed state after rule removal fails. In this case, #LSDNE_INCONSISTENT is returned and the model
 * is considered to be in an inconsistent state. The only way to proceed is to tear down the whole model
 * and reconstruct it from scratch.
 *
 * @param ctx LSDN context.
 * @param cb Problem callback.
 * @param user User data for the problem callback.
 *
 * @retval LSDNE_OK Commit was successful. New network model is now active in kernel.
 * @retval LSDNE_VALIDATE Model validation found problems. Old network model remains active in kernel.
 * @retval LSDNE_COMMIT Errors were encountered during commit. Kernel is in mixed state, it is possible to retry.
 * @retval LSDNE_INCONSISTENT Errors were encountered when decommitting rules. Model state is inconsistent with kernel state. You have to
 * start over. */
lsdn_err_t lsdn_commit(struct lsdn_context *ctx, lsdn_problem_cb cb, void *user)
{
	/* Error handling strategy:
	 * If commit/decommit functions return void, it is their responsibility to report the problem and
	 * mark the object as inconsistent as needed. Typically, the will use mark_commit_err for that.
	 *
	 * If the functions return lsdn_err_t, the caller has the responsibility to report the problem.
	 * Commit functions can return any errors (typically LSDNE_NETLINK, LSDNE_INCONSISTENT or LSDNE_NOMEM),
	 * while decommit functions are allowed to only return LSDNE_INCONSISTENT.
	 */
	trigger_startup_hooks(ctx);

	lsdn_err_t err = lsdn_validate(ctx, cb, user);
	if(err != LSDNE_OK)
		return err;

	err = lsdn_context_ensure_socket(ctx);
	if(err != LSDNE_OK) {
		lsdn_problem_report(ctx, LSDNP_NO_NLSOCK, LSDNS_END);
		return err;
	}

	/* List of objects to process:
	 *	setting, network, phys, physical attachment, virt
	 * Settings, networks and attachments do not need to be committed in any way, but we must keep them
	 * alive until PAs and virts are deleted. */

	/********* Decommit phase **********/
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, n) {
		lsdn_foreach(n->virt_list, virt_entry, struct lsdn_virt, v) {
			if (ack_decommit(&v->state)) {
				decommit_virt(v);
				ack_delete(v, virt_do_free);
			}
		}
		lsdn_foreach(n->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
			if (ack_decommit(&pa->state)) {
				decommit_pa(pa);
				ack_delete(pa, pa_do_free);
			}
		}
		if (ack_decommit(&n->state))
			ack_delete(n, net_do_free);
	}

	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p){
		if (ack_decommit(&p->state))
			ack_delete(p, phys_do_free);
	}

	lsdn_foreach(ctx->settings_list, settings_entry, struct lsdn_settings, s) {
		if (ack_decommit(&s->state))
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
				commit_pa(pa);
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

	if (ctx->inconsistent)
		return LSDNE_INCONSISTENT;
	else if (ctx->problem_count > 0)
		return LSDNE_COMMIT;
	else
		return LSDNE_OK;
}
