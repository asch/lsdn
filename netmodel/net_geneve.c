#include "private/net.h"
#include "private/sbridge.h"
#include "private/nl.h"
#include "private/errors.h"
#include "include/lsdn.h"
#include "include/nettypes.h"
#include "include/errors.h"


/* Make sure the VXLAN interface operating in metadata mode for that UDP port exists. */
static lsdn_err_t geneve_use_stunnel(struct lsdn_phys_attachment *a)
{
	lsdn_err_t err = LSDNE_OK;
	struct lsdn_settings *s = a->net->settings;
	struct lsdn_context *ctx = s->ctx;
	struct lsdn_if *tunnel = &s->geneve.tunnel;
	struct lsdn_ruleset *rules_in = &s->geneve.ruleset_in;
	if (s->geneve.refcount == 0) {
		err = lsdn_link_geneve_create(
			ctx->nlsock, tunnel, lsdn_mk_iface_name(ctx), s->geneve.port, true);
		if (err != LSDNE_OK) {
			return err;
		}

		err = lsdn_link_set(ctx->nlsock, tunnel->ifindex, true);
		if (err != LSDNE_OK) {
			acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, tunnel));
			return err;
		}

		err = lsdn_prepare_rulesets(ctx, tunnel, rules_in, NULL);
		if (err != LSDNE_OK) {
			acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, tunnel));
			return err;
		}

		err = lsdn_sbridge_phys_if_init(
			ctx, &s->geneve.tunnel_sbridge, tunnel, true, rules_in);
		if (err != LSDNE_OK) {
			acc_inconsistent(&err, lsdn_link_delete(ctx->nlsock, tunnel));
			return err;
		}
	}
	s->geneve.refcount++;

	return err;
}

static void geneve_release_stunnel(struct lsdn_settings *s)
{
	// TODO: also handle problems here
	if (--s->geneve.refcount == 0) {
		lsdn_sbridge_phys_if_free(&s->geneve.tunnel_sbridge);
		if(!s->ctx->disable_decommit) {
			lsdn_err_t err = lsdn_link_delete(s->ctx->nlsock, &s->geneve.tunnel);
			if (err != LSDNE_OK)
				abort();
		}
		lsdn_ruleset_free(&s->geneve.ruleset_in);
		lsdn_if_free(&s->geneve.tunnel);
	}
}

static lsdn_err_t geneve_create_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_err_t err = geneve_use_stunnel(pa);
	if (err != LSDNE_OK)
		return err;
	err = lsdn_sbridge_init(pa->net->ctx, &pa->sbridge);
	if (err != LSDNE_OK) {
		geneve_release_stunnel(pa->net->settings);
		return err;
	}

	err = lsdn_sbridge_add_stunnel(
		&pa->sbridge, &pa->sbridge_if,
		&pa->net->settings->geneve.tunnel_sbridge, pa->net);
	if (err != LSDNE_OK) {
		geneve_release_stunnel(pa->net->settings);
		acc_inconsistent(&err, lsdn_sbridge_free(&pa->sbridge));
		return err;
	}
	return LSDNE_OK;
}

static lsdn_err_t geneve_destroy_pa(struct lsdn_phys_attachment *pa)
{
	lsdn_err_t err = LSDNE_OK;
	acc_inconsistent(&err, lsdn_sbridge_remove_stunnel(&pa->sbridge_if));
	acc_inconsistent(&err, lsdn_sbridge_free(&pa->sbridge));
	geneve_release_stunnel(pa->net->settings);
	return err;
}

static void geneve_validate_pa(struct lsdn_phys_attachment *a)
{
	if (!a->phys->attr_ip)
		lsdn_problem_report(
			a->phys->ctx, LSDNP_PHYS_NOATTR,
			LSDNS_ATTR, "ip",
			LSDNS_PHYS, a->phys,
			LSDNS_NET, a->net,
			LSDNS_END);
}

static void geneve_validate_virt(struct lsdn_virt *virt)
{
	if (!virt->attr_mac)
		lsdn_problem_report(
			virt->network->ctx, LSDNP_VIRT_NOATTR,
			LSDNS_ATTR, "mac",
			LSDNS_VIRT, virt,
			LSDNS_NET, virt->network,
			LSDNS_END);
}

static lsdn_err_t geneve_add_virt(struct lsdn_virt *virt)
{
	return lsdn_sbridge_add_virt(&virt->committed_to->sbridge, virt);
}

static lsdn_err_t geneve_remove_virt(struct lsdn_virt *virt)
{
	return lsdn_sbridge_remove_virt(virt);
}

static void set_geneve_metadata(struct lsdn_filter *f, uint16_t order, void *user)
{
	struct lsdn_remote_pa *pa = user;
	lsdn_action_set_tunnel_key(f, order,
		pa->local->net->vnet_id,
		pa->local->phys->attr_ip,
		pa->remote->phys->attr_ip);

}

static lsdn_err_t geneve_add_remote_pa (struct lsdn_remote_pa *pa)
{
	struct lsdn_action_desc *bra = &pa->sbridge_route.tunnel_action;
	bra->actions_count = 1;
	bra->fn = set_geneve_metadata;
	bra->user = pa;
	return lsdn_sbridge_add_route(&pa->local->sbridge_if, &pa->sbridge_route);
}

static lsdn_err_t geneve_remove_remote_pa (struct lsdn_remote_pa *pa)
{
	return lsdn_sbridge_remove_route(&pa->sbridge_route);
}

static lsdn_err_t geneve_add_remote_virt(struct lsdn_remote_virt *virt)
{
	return lsdn_sbridge_add_mac(&virt->pa->sbridge_route, &virt->sbridge_mac, *virt->virt->attr_mac);
}

static lsdn_err_t geneve_remove_remote_virt(struct lsdn_remote_virt *virt)
{
	return lsdn_sbridge_remove_mac(&virt->sbridge_mac);
}

static uint16_t geneve_get_port(struct lsdn_settings *s)
{
	return s->geneve.port;
}

struct lsdn_net_ops lsdn_net_geneve_ops = {
	.type = "geneve",
	.get_port = geneve_get_port,
	.create_pa = geneve_create_pa,
	.destroy_pa = geneve_destroy_pa,
	.add_virt = geneve_add_virt,
	.remove_virt = geneve_remove_virt,
	.add_remote_pa = geneve_add_remote_pa,
	.remove_remote_pa = geneve_remove_remote_pa,
	.add_remote_virt = geneve_add_remote_virt,
	.remove_remote_virt = geneve_remove_remote_virt,
	.validate_pa = geneve_validate_pa,
	.validate_virt = geneve_validate_virt
};

struct lsdn_settings *lsdn_settings_new_geneve(struct lsdn_context *ctx, uint16_t port)
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
	s->switch_type = LSDN_STATIC_E2E;
	s->nettype = LSDN_NET_GENEVE;
	s->ops = &lsdn_net_geneve_ops;
	s->geneve.port = port;
	s->geneve.refcount = 0;
	return s;
}
