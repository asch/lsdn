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
static void vlan_create_pa(struct lsdn_phys_attachment *p)
{
	// create the vlan interface
	lsdn_err_t err = lsdn_link_vlan_create(
		p->net->ctx->nlsock, &p->tunnel_if,
		p->phys->attr_iface, lsdn_mk_ifname(p->net->ctx), p->net->vnet_id);
	if(err != LSDNE_OK)
		abort();

	lsdn_lbridge_init(p->net->ctx, &p->lbridge);
	lsdn_lbridge_add(&p->lbridge, &p->lbridge_if, &p->tunnel_if);
}

/** Remove a machine from VLAN network.
 * Implements `lsdn_net_ops.destroy_pa`.
 *
 * Destroys the local Linux Bridge and possibly also TC rules. */
static void vlan_destroy_pa(struct lsdn_phys_attachment *p)
{
	lsdn_lbridge_remove(&p->lbridge_if);
	lsdn_lbridge_free(&p->lbridge);
	if(!p->net->ctx->disable_decommit) {
		int err = lsdn_link_delete(p->net->ctx->nlsock, &p->tunnel_if);
		if (err)
			abort();
	}
	lsdn_if_free(&p->tunnel_if);
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
	.create_pa = vlan_create_pa,
	.destroy_pa = vlan_destroy_pa,
	.add_virt = lsdn_lbridge_add_virt,
	.remove_virt = lsdn_lbridge_remove_virt,
	.compute_tunneling_overhead = vlan_tunneling_overhead
};

/** Create settings for a new direct network.
 * @return new `lsdn_settings` instance. The caller is responsible for freeing it. */
struct lsdn_settings *lsdn_settings_new_vlan(struct lsdn_context *ctx)
{
	struct lsdn_settings *s = malloc(sizeof(*s));
	if(!s)
		ret_ptr(ctx, NULL);

	lsdn_settings_init_common(s, ctx);
	s->ops = &lsdn_net_vlan_ops;
	s->nettype = LSDN_NET_VLAN;
	s->switch_type = LSDN_LEARNING;
	return s;
}
