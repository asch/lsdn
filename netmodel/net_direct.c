#include "private/net.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

static void direct_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err;
	lsdn_if_init(&a->tunnel_if);
	err = lsdn_if_set_name(&a->tunnel_if, a->phys->attr_iface);
	if (err != LSDNE_OK)
		abort();
	err = lsdn_if_resolve(&a->tunnel_if);
	if (err != LSDNE_OK)
		abort();

	// create the bridge and connect the otgouing interface to it
	lsdn_lbridge_init(a->net->ctx, &a->lbridge);
	lsdn_lbridge_add(&a->lbridge, &a->lbridge_if, &a->tunnel_if);
}

static void direct_destroy_pa(struct lsdn_phys_attachment *a)
{
	lsdn_lbridge_remove(&a->lbridge_if);
	lsdn_lbridge_free(&a->lbridge);
}

static struct lsdn_net_ops lsdn_net_direct_ops = {
	.create_pa = direct_create_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.destroy_pa = direct_destroy_pa,
};

struct lsdn_settings *lsdn_settings_new_direct(struct lsdn_context *ctx)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_list_init_add(&ctx->settings_list, &s->settings_entry);
	s->ctx = ctx;
	s->ops = &lsdn_net_direct_ops;
	s->nettype = LSDN_NET_DIRECT;
	s->switch_type = LSDN_LEARNING;
	s->state = LSDN_STATE_NEW;
	s->user_hooks = NULL;
	return s;
}
