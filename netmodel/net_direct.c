/** \file
 * Implementation of the "direct" network type. */
#include "private/net.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

/** Add a machine to direct network.
 * Implements `lsdn_net_ops.create_pa`.
 *
 * Sets up a tunnel interface and connects it to a Linux Bridge. */
static lsdn_err_t direct_create_pa(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err;
	lsdn_if_init(&a->tunnel_if);
	err = lsdn_if_set_name(&a->tunnel_if, a->phys->attr_iface);
	if (err != LSDNE_OK)
		return err;
	err = lsdn_if_resolve(&a->tunnel_if);
	if (err != LSDNE_OK) {
		lsdn_if_free(&a->tunnel_if);
		return err;
	}

	err = lsdn_lbridge_create_pa(a);
	if (err != LSDNE_OK) {
		lsdn_if_free(&a->tunnel_if);
		return err;
	}
	return LSDNE_OK;
}

static unsigned int direct_tunneling_overhead(struct lsdn_phys_attachment *pa)
{
	LSDN_UNUSED(pa);
	return 0;
}

/** Callbacks for direct network.
 * Adding and removing local virts entails adding to the local Linux Bridge,
 * so we are using functions from `lbridge.c`. */
static struct lsdn_net_ops lsdn_net_direct_ops = {
	.type = "direct",
	.create_pa = direct_create_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.destroy_pa = lsdn_lbridge_destroy_pa,
	.compute_tunneling_overhead = direct_tunneling_overhead
};

/** Create settings for a new direct network.
 * @return new `lsdn_settings` instance. */
struct lsdn_settings *lsdn_settings_new_direct(struct lsdn_context *ctx)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);
	lsdn_err_t err = lsdn_settings_init_common(s, ctx);
	assert(err != LSDNE_DUPLICATE);
	if (err == LSDNE_NOMEM) {
		free(s);
		ret_ptr(ctx, NULL);
	}
	s->ops = &lsdn_net_direct_ops;
	s->nettype = LSDN_NET_DIRECT;
	s->switch_type = LSDN_LEARNING;
	return s;
}
