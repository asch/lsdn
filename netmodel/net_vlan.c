/** \file
 * Implementation of a VLAN-based network. */
#include "private/net.h"
#include "private/lbridge.h"
#include "private/nl.h"
#include "include/lsdn.h"
#include "private/errors.h"

/** Add a machine to VLAN network.
 * Implements `lsdn_net_ops.create_pa`.
 *
 * Creates a dedicated VLAN interface for this machine, sets up
 * a Linux Bridge and adds the interface to it. */
static lsdn_err_t vlan_create_pa(struct lsdn_phys_attachment *p)
{
	lsdn_err_t err;
	lsdn_if_init(&p->tunnel_if);
	// create the vlan interface
	err = lsdn_link_vlan_create(
		p->net->ctx->nlsock, &p->tunnel_if,
		p->phys->attr_iface, lsdn_mk_iface_name(p->net->ctx), p->net->vnet_id);
	if(err != LSDNE_OK)
		return err;

	err = lsdn_lbridge_create_pa(p);
	if (err != LSDNE_OK) {
		if (lsdn_link_delete(p->net->ctx->nlsock, &p->tunnel_if) != LSDNE_OK)
			return LSDNE_INCONSISTENT;
		lsdn_if_free(&p->tunnel_if);
		return err;
	}
	return LSDNE_OK;
}

/** Compute tunneling overhead of VLAN tunnels. */
static unsigned int vlan_tunneling_overhead(struct lsdn_phys_attachment *pa)
{
	return 4;
}

/** Callbacks for VLAN network.
 * Adding and removing local virts entails adding to the local Linux Bridge,
 * so we are using functions from `lbridge.c`. */
static struct lsdn_net_ops lsdn_net_vlan_ops = {
	.type = "vlan",
	.create_pa = vlan_create_pa,
	.destroy_pa = lsdn_lbridge_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.compute_tunneling_overhead = vlan_tunneling_overhead
};

/** Create settings for a new VLAN network.
 * @return new `lsdn_settings` instance. The caller is responsible for freeing it. */
struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx)
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
	s->ops = &lsdn_net_vlan_ops;
	s->nettype = LSDN_NET_VLAN;
	s->switch_type = LSDN_LEARNING;
	return s;
}
