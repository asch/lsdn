#include "private/net.h"


struct lsdn_net* lsdn_net_new_common(
	struct lsdn_context *ctx,
	enum lsdn_nettype type, enum lsdn_switch stype)
{
	struct lsdn_net *net = malloc(sizeof(*net));
	if(!net)
		return NULL;
	net->ctx = ctx;
	net->switch_type = stype;
	net->nettype = type;

	lsdn_list_init_add(&ctx->networks_list, &net->networks_entry);
	lsdn_list_init(&net->attached_list);
	lsdn_list_init(&net->virt_list);
	return net;
}
