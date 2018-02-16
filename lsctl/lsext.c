/** \file
 * TCL extensions for lsctl language. */

#include "lsext.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "../netmodel/include/lsdn.h"
#include "../netmodel/include/rules.h"

static int tcl_error(Tcl_Interp *interp, const char *err) {
	Tcl_SetResult(interp, (char*) err, NULL);
	return TCL_ERROR;
}

enum scope {
	S_ROOT = 1,
	S_NET = 2,
	S_PHYS = 4,
	S_VIRT = 8
};
#define MAX_SCOPE_NESTING 3

struct tcl_ctx{
	struct lsdn_context *lsctx;
	struct lsdn_net *net;
	struct lsdn_phys *phys;
	struct lsdn_virt *virt;
	size_t stack_pos;
	enum scope scope_stack[MAX_SCOPE_NESTING];

	Tcl_Obj *str_split;
	Tcl_Obj *str_slash;
};

#define CMD(name) static int tcl_##name(struct tcl_ctx *ctx, Tcl_Interp *interp, int argc, Tcl_Obj *const argv[])

static void push_scope(struct tcl_ctx *ctx, enum scope s)
{
	ctx->scope_stack[++ctx->stack_pos] = s;
}

static void pop_scope(struct tcl_ctx *ctx)
{
	ctx->stack_pos--;
}

static enum scope get_scope(struct tcl_ctx *ctx)
{
	return ctx->scope_stack[ctx->stack_pos];
}

static int store_arg(ClientData d, Tcl_Obj *obj, void *dst_ptr)
{
	Tcl_Obj **dst_typed = (Tcl_Obj**) dst_ptr;
	*dst_typed = obj;
	return 1;
}

/** Check that command has valid scoping.
 *
 * Only the direct enclosing scope is checked.
 *
 * @param allowed List of scopes that are allowed for this command.
 * @return Tcl status.
 */
static int check_scope(Tcl_Interp *interp, struct tcl_ctx *ctx, enum scope allowed)
{
	enum scope current = get_scope(ctx);
	if ((current == S_ROOT) && !(allowed & S_ROOT))
		return tcl_error(interp, "command is not allowed inside root scope");
	if ((current == S_NET) && !(allowed & S_NET))
		return tcl_error(interp, "command is not allowed inside 'net' scope");
	if ((current == S_PHYS) && !(allowed & S_PHYS))
		return tcl_error(interp, "command is not allowed inside 'phys' scope");
	if ((current == S_VIRT) && !(allowed & S_VIRT))
		return tcl_error(interp, "command is not allowed inside 'virt' scope");
	return TCL_OK;
}

static Tcl_Obj *split_slashes(Tcl_Interp *interp, struct tcl_ctx *ctx, Tcl_Obj *obj)
{
	Tcl_Obj* v[] = {ctx->str_split, obj, ctx->str_slash};
	Tcl_EvalObjv(interp, 3, v, 0);
	return Tcl_GetObjResult(interp);
}

/** Get a net argument, either explicit or from scope.
 * Find out if the command is either nested inside a `net {}` scope or if it has explicit
 * `-net` keyword argument.
 *
 * The caller is still responsible for parsing the keyword arguments using `Tcl_ParseArgsObjv` and
 * only passes in the extracted value for `-net`.
 *
 * @param arg The `-net` keyword argument value.
 * @param net Output value. Undefined if `TCL_ERROR` is returned. May be null if `needed` is `false`.
 * @param needed Cause a `TCL_ERROR` if neither the `-net` argument is given or scope is active.
 * @return Tcl status.
 */
static int resolve_net_arg(
	Tcl_Interp *interp, struct tcl_ctx *ctx,
	const char *arg, struct lsdn_net **net, bool needed)
{
	if (arg && ctx->net) {
		return tcl_error(interp, "-net argument is not allowed inside net scope");
	} else if (ctx->net) {
		*net = ctx->net;
	} else if (arg) {
		*net = lsdn_net_by_name(ctx->lsctx, arg);
		if (!*net)
			return tcl_error(interp, "can not find network");
	} else {
		*net = NULL;
	}
	if (!net && needed)
		return tcl_error(interp, "network must be specified, either using -net argument or from net scope");
	return TCL_OK;
}

/** Get a phys argument, either explicit or from scope.
 * @see resolve_phys_arg
 */
static int resolve_phys_arg(
	Tcl_Interp *interp, struct tcl_ctx *ctx,
	const char *arg, struct lsdn_phys **phys, bool needed)
{
	if (arg && ctx->phys) {
		return tcl_error(interp, "-phys argument is not allowed inside phys scope");
	} else if (ctx->phys) {
		*phys = ctx->phys;
	} else if (arg) {
		*phys = lsdn_phys_by_name(ctx->lsctx, arg);
		if (!*phys)
			return tcl_error(interp, "can not find phys");
	} else {
		*phys = NULL;
	}
	if (!phys && needed)
		return tcl_error(interp, "phys must be specified, either using -phys argument or from phys scope");
	return TCL_OK;
}

static int settings_common(Tcl_Interp *interp, struct lsdn_settings *settings, const char *name)
{
	lsdn_err_t err = lsdn_settings_set_name(settings, name ? name : "default");
	if (err != LSDNE_OK) {
		if (err == LSDNE_DUPLICATE) {
			if (name)
				return tcl_error(interp, "Name already exists");
			else
				return tcl_error(interp, "Default settings already exist");
		} else abort();
	}

	return TCL_OK;
}

CMD(settings_direct)
{
	const char *name = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_direct(ctx->lsctx);
	return settings_common(interp, settings, name);
}

CMD(settings_vlan)
{
	const char *name = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_vlan(ctx->lsctx);
	return settings_common(interp, settings, name);
}

CMD(settings_vxlan_e2e)
{
	const char *name = NULL;
	int port = 4789;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_INT, "-port", NULL, &port},
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_END}
	};
	argc--; argv++;
	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_vxlan_e2e(ctx->lsctx, port);
	return settings_common(interp, settings, name);
}

CMD(settings_vxlan_mcast)
{
	const char* name = NULL;
	const char* ip;
	lsdn_ip_t ip_parsed;
	int port = 4789;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-mcastIp", NULL, &ip},
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_INT, "-port", NULL, &port},
		{TCL_ARGV_END}
	};
	argc--; argv++;
	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	if(!ip)
		return tcl_error(interp, "vxlan multicast require the -mcastIp argument");
	if(lsdn_parse_ip(&ip_parsed, ip) != LSDNE_OK)
		return tcl_error(interp, "mcastIp is not a valid ip address");

	struct lsdn_settings * settings = lsdn_settings_new_vxlan_mcast(ctx->lsctx, ip_parsed, port);
	return settings_common(interp, settings, name);
}

CMD(settings_vxlan_static)
{
	int port = 4789;
	const char *name = NULL;
	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_INT, "-port", NULL, &port},
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_vxlan_static(ctx->lsctx, port);
	return settings_common(interp, settings, name);
}

CMD(settings_geneve)
{
	int port = 6081;
	const char *name = NULL;
	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_INT, "-port", NULL, &port},
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_geneve(ctx->lsctx, port);
	return settings_common(interp, settings, name);
}

CMD(settings)
{
	int type;
	static const char *type_names[] = {
		"direct", "vlan", "vxlan/mcast", "vxlan/static", "vxlan/e2e", "geneve", NULL};
	enum types {
		T_DIRECT, T_VLAN, T_VXLAN_MCAST, T_VXLAN_STATIC, T_VXLAN_E2E, T_GENEVE};

	if(argc < 2) {
		Tcl_WrongNumArgs(interp, 1, argv, "type");
		return TCL_ERROR;
	}

	if(check_scope(interp, ctx, S_ROOT))
		return TCL_ERROR;

	if(Tcl_GetIndexFromObj(interp, argv[1], type_names, "type", 0, &type) != TCL_OK)
		return TCL_ERROR;

	switch(type){
		case T_DIRECT:
			return tcl_settings_direct(ctx, interp, argc, argv);
		case T_VLAN:
			return tcl_settings_vlan(ctx, interp, argc, argv);
		case T_VXLAN_MCAST:
			return tcl_settings_vxlan_mcast(ctx, interp, argc, argv);
		case T_VXLAN_STATIC:
			return tcl_settings_vxlan_static(ctx, interp, argc, argv);
		case T_VXLAN_E2E:
			return tcl_settings_vxlan_e2e(ctx, interp, argc, argv);
		case T_GENEVE:
			return tcl_settings_geneve(ctx, interp, argc, argv);
		default: abort();
	}

	return TCL_OK;
}

CMD(net)
{
	if (check_scope(interp, ctx, S_PHYS | S_ROOT) != TCL_OK)
		return TCL_ERROR;

	// TODO net id range 0 .. 2**32 - 1
	int vnet_id = 0;
	const char *settings_name = NULL;
	const char *phys = NULL;
	struct lsdn_phys *phys_parsed = NULL;
	int net_remove = 0;
	Tcl_Obj **pos_args = NULL;
	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_INT, "-vid", NULL, &vnet_id},
		{TCL_ARGV_STRING, "-settings", NULL, &settings_name},
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
		{TCL_ARGV_CONSTANT, "-remove", (void *) 1, &net_remove},
		{TCL_ARGV_END}
	};

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args) != TCL_OK)
		return TCL_ERROR;

	if(argc != 3 && argc != 2) {
		Tcl_WrongNumArgs(interp, 1, argv, "name [contents]");
		ckfree(pos_args);
		return TCL_ERROR;
	}

	struct lsdn_net *net = lsdn_net_by_name(ctx->lsctx, Tcl_GetString(pos_args[1]));

	if (net_remove) {
		if (!net) {
			ckfree(pos_args);
			return tcl_error(interp, "net not found, can not remove net");
		}
		lsdn_net_free(net);
		ckfree(pos_args);
		return TCL_OK;
	}
	if(resolve_phys_arg(interp, ctx, phys, &phys_parsed, false)) {
		ckfree(pos_args);
		return TCL_ERROR;
	}
	if (!vnet_id)
		vnet_id = strtol(Tcl_GetString(pos_args[1]), NULL, 10);
	if(!net) {
		struct lsdn_settings *s;
		if (!settings_name) {
			s = lsdn_settings_by_name(ctx->lsctx, "default");
			if (!s) {
				ckfree(pos_args);
				return tcl_error(interp, "Please define default network settings first");
			}
		} else {
			s = lsdn_settings_by_name(ctx->lsctx, settings_name);
			if (!s) {
				ckfree(pos_args);
				return tcl_error(interp, "settings not found");
			}
		}

		net = lsdn_net_new(s, vnet_id);
	} else {
		if (settings_name) {
			ckfree(pos_args);
			return tcl_error(interp, "Can not change settings, the network already exists");
		}
	}

	if(phys_parsed)
		lsdn_phys_attach(phys_parsed, net);

	int r = TCL_OK;
	lsdn_net_set_name(net, Tcl_GetString(pos_args[1]));
	if (argc == 3) {
		push_scope(ctx, S_NET);
		ctx->net = net;
		r = Tcl_EvalObj(interp, pos_args[2]);
		ctx->net = NULL;
		pop_scope(ctx);
	}

	ckfree(pos_args);
	return r;
}

CMD(virt)
{
	if (check_scope(interp, ctx, S_PHYS | S_NET | S_ROOT) != TCL_OK)
		return TCL_ERROR;

	const char *mac = NULL;
	lsdn_mac_t mac_parsed;
	const char *net = NULL;
	struct lsdn_net *net_parsed = NULL;
	const char *phys = NULL;
	struct lsdn_phys *phys_parsed = NULL;
	const char *iface = NULL;
	const char *name = NULL;
	int virt_remove = 0;
	int mac_clear = 0;
	struct lsdn_virt *virt = NULL;
	Tcl_Obj **pos_args = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-mac", NULL, &mac},
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
		{TCL_ARGV_STRING, "-net", NULL, &net},
		{TCL_ARGV_STRING, "-if", NULL, &iface},
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_CONSTANT, "-remove", (void *) 1, &virt_remove},
		{TCL_ARGV_CONSTANT, "-macClear", (void *) 1, &mac_clear},
		{TCL_ARGV_END}
	};

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args) != TCL_OK)
		return TCL_ERROR;

	if(argc != 2 && argc != 1) {
		Tcl_WrongNumArgs(interp, 1, argv, " contents");
		ckfree(pos_args);
		return TCL_ERROR;
	}

	if(resolve_net_arg(interp, ctx, net, &net_parsed, true)) {
		ckfree(pos_args);
		return TCL_ERROR;
	}

	if (mac && mac_clear) {
		ckfree(pos_args);
		return tcl_error(interp, "can not clear and set MAC address at the same time");
	}

	virt = lsdn_virt_by_name(net_parsed, name);
	if (virt_remove) {
		if (!virt) {
			ckfree(pos_args);
			return tcl_error(interp, "virt not found, can not remove virt");
		}
		lsdn_virt_free(virt);
		ckfree(pos_args);
		return TCL_OK;
	}

	if(resolve_phys_arg(interp, ctx, phys, &phys_parsed, false)) {
		ckfree(pos_args);
		return TCL_ERROR;
	}

	if (!virt) {
		virt = lsdn_virt_new(net_parsed);
	} else {
		if(net_parsed != lsdn_virt_get_net(virt)) {
			ckfree(pos_args);
			return tcl_error(interp, "Can not change network for existing virt");
		}
	}

	if (mac) {
		if(lsdn_parse_mac(&mac_parsed, mac) != LSDNE_OK) {
			ckfree(pos_args);
			return tcl_error(interp, "mac address is in invalid format");
		}
	}

	if(!net_parsed) {
		ckfree(pos_args);
		return tcl_error(interp, "virt must either have -net argument or be in net scope");
	}

	if (name)
		lsdn_virt_set_name(virt, name);
	if(mac)
		lsdn_virt_set_mac(virt, mac_parsed);
	else if (mac_clear)
		lsdn_virt_clear_mac(virt);
	if(phys_parsed)
		lsdn_virt_connect(virt, phys_parsed, iface);

	int r = TCL_OK;
	if (argc == 2) {
		push_scope(ctx, S_VIRT);
		ctx->virt = virt;
		r = Tcl_EvalObj(interp, pos_args[1]);
		ctx->virt = NULL;
		pop_scope(ctx);
	}

	ckfree(pos_args);
	return r;
}

static int parse_mac_match(
	struct tcl_ctx *ctx, Tcl_Interp *interp, Tcl_Obj *obj,
	lsdn_mac_t *value, lsdn_mac_t* mask)
{
	int r =  TCL_OK;
	obj = split_slashes(interp, ctx, obj);
	Tcl_IncrRefCount(obj);
	int parts;
	if (Tcl_ListObjLength(interp, obj, &parts) != TCL_OK) {
		r = TCL_ERROR;
		goto err;
	}

	if (parts == 1 || parts == 2) {
		Tcl_Obj *value_obj;
		r = Tcl_ListObjIndex(interp, obj, 0, &value_obj);
		assert (r == TCL_OK);
		lsdn_err_t ls_err = lsdn_parse_mac(value, Tcl_GetString(value_obj));
		if (ls_err != LSDNE_OK) {
			r = tcl_error(interp, "can not parse mac address");
			goto err;
		}

		if (parts == 2) {
			Tcl_Obj *mask_obj;
			r = Tcl_ListObjIndex(interp, obj, 0, &mask_obj);
			assert (r == TCL_OK);
			ls_err = lsdn_parse_mac(mask, Tcl_GetString(mask_obj));
			if (ls_err != LSDNE_OK) {
				r = tcl_error(interp, "can not parse mac address mask");
				goto err;
			}
		} else {
			*mask = lsdn_single_mac_mask;
		}
	} else {
		r = tcl_error(interp, "Mac match has too many parts. Expected either a single mac, or mac-value/mac-mask pair.");
		goto err;
	}
	err:
	Tcl_DecrRefCount(obj);
	return r;
}

static int parse_ip_match(
	struct tcl_ctx *ctx, Tcl_Interp *interp, Tcl_Obj *obj,
	lsdn_ip_t *value, lsdn_ip_t* mask)
{
	int r =  TCL_OK;
	obj = split_slashes(interp, ctx, obj);
	Tcl_IncrRefCount(obj);
	int parts;
	if (Tcl_ListObjLength(interp, obj, &parts) != TCL_OK) {
		r = TCL_ERROR;
		goto err;
	}

	if (parts == 1 || parts == 2) {
		Tcl_Obj *value_obj;
		r = Tcl_ListObjIndex(interp, obj, 0, &value_obj);
		assert (r == TCL_OK);
		lsdn_err_t ls_err = lsdn_parse_ip(value, Tcl_GetString(value_obj));
		if (ls_err != LSDNE_OK) {
			r = tcl_error(interp, "can not parse mac address");
			goto err;
		}

		if (parts == 2) {
			Tcl_Obj *mask_obj;
			int prefix;
			r = Tcl_ListObjIndex(interp, obj, 1, &mask_obj);
			assert (r == TCL_OK);
			r = Tcl_GetIntFromObj(interp, mask_obj, &prefix);
			if (r == TCL_OK) {
				/* the second part is network prefix */
				if (!lsdn_is_prefix_valid(value->v, prefix)) {
					r = tcl_error(interp, "the network prefix has invalid size for given IP version");
					goto err;
				}
				*mask = lsdn_ip_mask_from_prefix(value->v, prefix);
			} else {
				r = TCL_OK;
				/* the second part is mask or malformed */
				ls_err = lsdn_parse_ip(mask, Tcl_GetString(mask_obj));
				if (ls_err != LSDNE_OK) {
					r = tcl_error(interp, "can not parse mac address mask");
					goto err;
				}
			}
		} else {
			if (value->v == LSDN_IPv4) {
				*mask = lsdn_single_ipv4_mask;
			} else {
				*mask = lsdn_single_ipv6_mask;
			}
		}
	} else {
		r = tcl_error(interp,
			"Mac match has too many parts. Expected either a single ip, "
			"ip-value/ip-mask pair or ip-value/prefix pair.");
		goto err;
	}

	if (value->v != mask->v)
		r = tcl_error(interp,"The IP protocol versions for the value and mask do not match");
err:
	Tcl_DecrRefCount(obj);
	return r;
}
CMD(rule)
{
	/* example: rule in 10 drop -srcIp 192.168.24.0/24 */
	if (check_scope(interp, ctx, S_VIRT) != TCL_OK)
		return TCL_ERROR;

	Tcl_Obj *src_mac = NULL;
	lsdn_mac_t src_mac_value, src_mac_mask;
	Tcl_Obj *dst_mac = NULL;
	lsdn_mac_t dst_mac_value, dst_mac_mask;
	Tcl_Obj *src_ip = NULL;
	lsdn_ip_t src_ip_value, src_ip_mask;
	Tcl_Obj *dst_ip = NULL;
	lsdn_ip_t dst_ip_value, dst_ip_mask;

	int prio;

	const char* direction_names[] = {"in", "out"};
	enum lsdn_direction direction_values[] = {LSDN_IN, LSDN_OUT};
	int direction_index = 0;
	enum lsdn_direction direction;
	const char* action = NULL;

	if (argc < 4) {
		Tcl_WrongNumArgs(interp, 1, argv, " direction prio action");
		return TCL_ERROR;
	}

	if(Tcl_GetIndexFromObj(interp, argv[1], direction_names, "direction", 0, &direction_index) != TCL_OK)
		return TCL_ERROR;
	direction = direction_values[direction_index];

	if(Tcl_GetIntFromObj(interp, argv[2], &prio) != TCL_OK)
		return TCL_ERROR;

	if (prio < LSDN_VR_PRIO_MIN || prio >= LSDN_VR_PRIO_MAX) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"Rule priorities must be between %d and %d", LSDN_VR_PRIO_MIN, LSDN_VR_PRIO_MAX));
		return TCL_ERROR;
	}

	action = Tcl_GetString(argv[3]);

	argc -= 3;
	argv += 3;

	if (strcmp(action, "drop") != 0)
		return tcl_error(interp, "only the drop action is supported");

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_FUNC, "-srcMac", store_arg, &src_mac},
		{TCL_ARGV_FUNC, "-dstMac", store_arg, &dst_mac},
		{TCL_ARGV_FUNC, "-srcIp", store_arg, &src_ip},
		{TCL_ARGV_FUNC, "-dstIp", store_arg, &dst_ip},
		{TCL_ARGV_END}
	};

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	size_t count = !!src_mac + !!dst_mac + !!src_ip + !!dst_ip;
	if (count > LSDN_MAX_MATCHES) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Maximum number of match targets per rule is: %d", LSDN_MAX_MATCHES));
		return TCL_ERROR;
	}

	if (src_mac) {
		if (parse_mac_match(ctx, interp, src_mac, &src_mac_value, &src_mac_mask) != TCL_OK)
			return TCL_ERROR;
	}

	if (dst_mac) {
		if (parse_mac_match(ctx, interp, dst_mac, &dst_mac_value, &dst_mac_mask) != TCL_OK)
			return TCL_ERROR;
	}

	if (src_ip) {
		if (parse_ip_match(ctx, interp, src_ip, &src_ip_value, &src_ip_mask) != TCL_OK)
			return TCL_ERROR;
	}

	if (dst_ip) {
		if (parse_ip_match(ctx, interp, dst_ip, &dst_ip_value, &dst_ip_mask) != TCL_OK)
			return TCL_ERROR;
	}

	struct lsdn_vr *vr = lsdn_vr_new(ctx->virt, prio, direction, &lsdn_vr_drop);
	if (src_mac)
		lsdn_vr_add_masked_src_mac(vr, src_mac_value, src_mac_mask);
	if (dst_mac)
		lsdn_vr_add_masked_dst_mac(vr, src_mac_value, src_mac_mask);
	if (src_ip)
		lsdn_vr_add_masked_src_ip(vr, src_ip_value, src_ip_mask);
	if (dst_ip)
		lsdn_vr_add_masked_dst_ip(vr, dst_ip_value, dst_ip_mask);
	return TCL_OK;
}

struct unit {
	const char *name;
	double scale;
};

static const struct unit utab_bytes[] = {
/* we use units consistent with tc */
	{"", 1},
	{"kb", 1024},
	{"mb", 1024*1024},
	{"gb", 1024*1024*1024},
	{"bit", 1/8.},
	{"kbit", 1024/8.},
	{"mbit", 1024*1024/8.},
	{"gbit", 1024*1024*1024/8.},
	{NULL, 0},
};

static int parse_units(Tcl_Interp *interp, const char *value, float *out, const struct unit *unit_tab)
{
	/* Pre-validate the floating point format to (\d+)(.\d+)?<unit>, stricter than strtod */
	const char *c = value;
	/* We assume C locale here */
	if(!isdigit(*c))
		return tcl_error(interp, "numeric value must start with a digit");
	while(isdigit(*c))
		c++;
	if (*c == '.') {
		c++;
		if(!isdigit(*c))
			return tcl_error(interp, "digit must follow after '.'");
		while(isdigit(*c))
			c++;
	}
	const char *unit_start = c;
	while(isalpha(*c))
		c++;
	if (*c)
		return tcl_error(interp, "garbage foun after numeric value");

	for(;unit_tab->name; unit_tab++) {
		if (strcmp(unit_tab->name, unit_start) == 0)
			break;
	}
	if (!unit_tab->name)
		return tcl_error(interp, "unknown unit");
	*out = strtof(value, NULL) * unit_tab->scale;
	return TCL_OK;
}

CMD(rate)
{
	/* example rate in -avg 0.5mbit -burst 1mb -burstRate 10mb */
	if (check_scope(interp, ctx, S_VIRT) != TCL_OK)
		return TCL_ERROR;

	const char* direction_names[] = {"in", "out"};
	enum lsdn_direction direction_values[] = {LSDN_IN, LSDN_OUT};
	int direction_index = 0;
	enum lsdn_direction direction;

	const char *avg = NULL;
	const char *burst_size = NULL;
	const char *burst_rate = NULL;

	if (argc < 2) {
		Tcl_WrongNumArgs(interp, 1, argv, " direction");
		return TCL_ERROR;
	}

	if(Tcl_GetIndexFromObj(interp, argv[1], direction_names, "direction", 0, &direction_index) != TCL_OK)
		return TCL_ERROR;
	direction = direction_values[direction_index];
	argc -= 1;
	argv += 1;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-avg", NULL, &avg},
		{TCL_ARGV_STRING, "-burstRate", NULL, &burst_rate},
		{TCL_ARGV_STRING, "-burst", NULL, &burst_size},
		{TCL_ARGV_END}
	};

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	lsdn_qos_rate_t rate;
	if (avg) {
		if (parse_units(interp, avg, &rate.avg_rate, utab_bytes) != TCL_OK)
			return TCL_ERROR;
	} else {
		return tcl_error(interp, "-avg argument must be specified");
	}

	float burst_size_f;
	if (burst_size) {
		if (parse_units(interp, burst_size, &burst_size_f, utab_bytes) != TCL_OK)
			return TCL_ERROR;
	} else {
		return tcl_error(interp, "-burst argument must be specified");
	}
	rate.burst_size = burst_size_f;

	if (burst_rate) {
		if (parse_units(interp, burst_rate, &rate.burst_rate, utab_bytes) != TCL_OK)
			return TCL_ERROR;
	} else {
		rate.burst_rate = 0;
	}

	bool no_args = !avg && !burst_size && !burst_rate;
	if (direction == LSDN_IN) {
		if (no_args)
			lsdn_virt_clear_rate_in(ctx->virt);
		else
			lsdn_virt_set_rate_in(ctx->virt, rate);
	} else {
		if (no_args)
			lsdn_virt_clear_rate_out(ctx->virt);
		else
			lsdn_virt_set_rate_out(ctx->virt, rate);
	}

	return TCL_OK;
}

CMD(flushVr)
{
	if (check_scope(interp, ctx, S_VIRT) != TCL_OK)
		return TCL_ERROR;
	lsdn_vrs_free_all(ctx->virt);
	return TCL_OK;
}

CMD(phys)
{
	if (check_scope(interp, ctx, S_NET | S_ROOT) != TCL_OK)
		return TCL_ERROR;

	const char *name = NULL;
	const char *iface = NULL;
	const char *ip = NULL;
	const char *net = NULL;
	struct lsdn_net *net_parsed = NULL;
	int phys_remove = 0;
	int iface_clear = 0;
	int ip_clear = 0;
	lsdn_ip_t ip_parsed;
	Tcl_Obj **pos_args = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_STRING, "-if", NULL, &iface},
		{TCL_ARGV_STRING, "-ip", NULL, &ip},
		{TCL_ARGV_STRING, "-net", NULL, &net},
		{TCL_ARGV_CONSTANT, "-remove", (void *) 1, &phys_remove},
		{TCL_ARGV_CONSTANT, "-ifClear", (void *) 1, &iface_clear},
		{TCL_ARGV_CONSTANT, "-ipClear", (void *) 1, &ip_clear},
		{TCL_ARGV_END}
	};
	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args))
		return TCL_ERROR;

	if (iface && iface_clear) {
		ckfree(pos_args);
		return tcl_error(interp, "can not clear and set interface at the same time");
	}

	if (ip && ip_clear) {
		ckfree(pos_args);
		return tcl_error(interp, "can not clear and set IP address at the same time");
	}

	struct lsdn_phys *phys = lsdn_phys_by_name(ctx->lsctx, name);
	if (phys_remove) {
		if (!phys) {
			ckfree(pos_args);
			return tcl_error(interp, "phys not found, can not remove phys");
		}
		lsdn_phys_free(phys);
		ckfree(pos_args);
		return TCL_OK;
	}

	if(resolve_net_arg(interp, ctx, net, &net_parsed, false)) {
		ckfree(pos_args);
		return TCL_ERROR;
	}

	if (!phys)
		phys = lsdn_phys_new(ctx->lsctx);

	if (ip) {
		if(lsdn_parse_ip(&ip_parsed, ip) != LSDNE_OK) {
			ckfree(pos_args);
			return tcl_error(interp, "ip address not in valid format");
		}
	}

	if (name)
		lsdn_phys_set_name(phys, name);
	if (iface)
		lsdn_phys_set_iface(phys, iface);
	else if (iface_clear)
		lsdn_phys_clear_iface(phys);
	if (ip)
		lsdn_phys_set_ip(phys, ip_parsed);
	else if (ip_clear)
		lsdn_phys_clear_ip(phys);
	if (net_parsed)
		lsdn_phys_attach(phys, net_parsed);

	push_scope(ctx, S_PHYS);
	ctx->phys = phys;
	int r = TCL_OK;
	if (argc == 2) {
		r = Tcl_EvalObj(interp, pos_args[1]);
	} else if (argc != 1) {
		Tcl_WrongNumArgs(interp, 1, argv, "contents");
		return TCL_ERROR;
	}
	ctx->phys = NULL;
	pop_scope(ctx);
	ckfree(pos_args);
	return r;
}

CMD(commit)
{
	if(check_scope(interp, ctx, S_ROOT))
		return TCL_ERROR;

	if(lsdn_commit(ctx->lsctx, lsdn_problem_stderr_handler, NULL) != LSDNE_OK)
		return tcl_error(interp, "commit error");
	return TCL_OK;
}

CMD(validate)
{
	if(check_scope(interp, ctx, S_ROOT))
		return TCL_ERROR;

	if(lsdn_validate(ctx->lsctx, lsdn_problem_stderr_handler, NULL) != LSDNE_OK)
		return tcl_error(interp, "commit error");
	return TCL_OK;
}

CMD(claimLocal)
{
	if (check_scope(interp, ctx, S_ROOT | S_PHYS))
		return TCL_ERROR;

	const char *phys = NULL;
	struct lsdn_phys *phys_parsed = NULL;
	Tcl_Obj **pos_args = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
		{TCL_ARGV_END}
	};

	if (Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args) != TCL_OK)
		return TCL_ERROR;

	if (resolve_phys_arg(interp, ctx, phys, &phys_parsed, false)) {
		ckfree(pos_args);
		return TCL_ERROR;
	}

	/* too much specified */
	if (phys_parsed && (argc > 1)) {
		ckfree(pos_args);
		if (ctx->phys)
			return tcl_error(interp, "claimLocal in phys scope does not expect a list of physes");
		else
			return tcl_error(interp, "claimLocal with -phys argument does not expect a list of physes");
	}

	/* nothing specified */
	if (!phys_parsed && (argc <= 1)) {
		ckfree(pos_args);
		return tcl_error(interp, "expected either a list of physes, a phys scope or -phys argument");
	}

	if (phys_parsed) {
		/* the -phys or phys scope form */
		lsdn_phys_claim_local(phys_parsed);
	} else {
		/* the positional arguments form */
		for (int i = 1; i<argc; i++) {
			phys = Tcl_GetString(pos_args[i]);
			phys_parsed = lsdn_phys_by_name(ctx->lsctx, phys);
			if (!phys_parsed) {
				ckfree(pos_args);
				return tcl_error(interp, "can not find phys");
			}
			lsdn_phys_claim_local(phys_parsed);
		}
	}
	return TCL_OK;
}

CMD(cleanup)
{
	if(check_scope(interp, ctx, S_ROOT) != TCL_OK)
		return TCL_ERROR;

	if(argc != 1) {
		Tcl_WrongNumArgs(interp, 1, argv, "");
		return TCL_ERROR;
	}
	lsdn_context_cleanup(ctx->lsctx, lsdn_problem_stderr_handler, NULL);
	ctx->lsctx = lsdn_context_new("lsdn");
	return TCL_OK;
}

CMD(free)
{
	if(check_scope(interp, ctx, S_ROOT) != TCL_OK)
		return TCL_ERROR;

	if(argc != 1) {
		Tcl_WrongNumArgs(interp, 1, argv, "");
		return TCL_ERROR;
	}
	lsdn_context_free(ctx->lsctx);
	ctx->lsctx = lsdn_context_new("lsdn");
	return TCL_OK;
}

static int attach_or_detach(
	Tcl_Interp* interp, struct tcl_ctx *ctx, int argc, Tcl_Obj *const argv[],
	lsdn_err_t (*cb)(struct lsdn_phys*, struct lsdn_net*))
{
	if(check_scope(interp, ctx, S_ROOT | S_PHYS | S_NET) != TCL_OK)
		return TCL_ERROR;

	const char *phys = NULL;
	struct lsdn_phys *phys_parsed = NULL;
	const char *net = NULL;
	struct lsdn_net *net_parsed = NULL;
	Tcl_Obj** pos_args = NULL;
	int r = TCL_ERROR;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
		{TCL_ARGV_STRING, "-net", NULL, &net},
		{TCL_ARGV_END}
	};

	if (Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args) != TCL_OK)
		goto err;

	if (resolve_net_arg(interp, ctx, net, &net_parsed, false))
		goto err;

	if (resolve_phys_arg(interp, ctx, phys, &phys_parsed, false))
		goto err;

	if (net_parsed && phys_parsed) {
		if (argc == 1) {
			cb(phys_parsed, net_parsed);
			r = TCL_OK;
		} else {
			r = tcl_error(interp, "Can not specify all three: -phys (or phys scope), -net (or net scope) and positional arguments");
		}
	} else if (net_parsed) {
		if (argc > 1) {
			for (int i = 1; i<argc; i++)
			{
				struct lsdn_phys *p = lsdn_phys_by_name(ctx->lsctx, Tcl_GetString(pos_args[i]));
				if (!p) {
					r = tcl_error(interp, "phys not found");
					goto err;
				}
				cb(p, net_parsed);
			}
			r = TCL_OK;
		} else {
			r = tcl_error(interp, "Need to specify a phys, either as -phys argument, phys scope or positional argument");
		}
	} else if (phys_parsed) {
		if (argc > 1) {
			for (int i = 1; i<argc; i++)
			{
				struct lsdn_net *n = lsdn_net_by_name(ctx->lsctx, Tcl_GetString(pos_args[i]));
				if (!n) {
					r = tcl_error(interp, "net not found");
					goto err;
				}
				cb(phys_parsed, n);
			}
			r = TCL_OK;
		} else {
			r = tcl_error(interp, "Need to specify a net, either as -net argument, net scope or positional argument");
		}
	} else {
		r = tcl_error(interp, "command requires a -phys and -virt to attach (one of them can be a list of positional arguments)");
	}

	err:
	ckfree(pos_args);
	return r;
}

CMD(attach)
{
	return attach_or_detach(interp, ctx, argc, argv, lsdn_phys_attach);
}

static lsdn_err_t detach_wrapper(struct lsdn_phys *p, struct lsdn_net *n)
{
	lsdn_phys_detach(p, n);
	return LSDNE_OK;
}

CMD(detach)
{
	return attach_or_detach(interp, ctx, argc, argv, detach_wrapper);
}

static struct tcl_ctx default_ctx;

#define REGISTER(name) Tcl_CreateObjCommand(interp, "lsdn::" #name, (Tcl_ObjCmdProc*) tcl_##name, ctx, NULL)

int Lsctl_Init(Tcl_Interp *interp)
{
	return register_lsdn_tcl(interp);
}

int register_lsdn_tcl(Tcl_Interp *interp)
{
	Tcl_Namespace *ns;
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		 return TCL_ERROR;
	}

	if ((ns = Tcl_CreateNamespace(interp, "lsdn", NULL, NULL)) == NULL) {
		return TCL_ERROR;
	}

	struct tcl_ctx *ctx = &default_ctx;
	ctx->lsctx = lsdn_context_new("lsdn");
	ctx->stack_pos = 0;
	ctx->scope_stack[0] = S_ROOT;
	ctx->str_split = Tcl_NewStringObj("::split", -1);
	Tcl_IncrRefCount(ctx->str_split);
	ctx->str_slash = Tcl_NewStringObj("/", -1);
	Tcl_IncrRefCount(ctx->str_slash);

	lsdn_context_abort_on_nomem(ctx->lsctx);
	REGISTER(settings);
	REGISTER(net);
	REGISTER(virt);
	REGISTER(phys);
	REGISTER(commit);
	REGISTER(validate);
	REGISTER(claimLocal);
	REGISTER(cleanup);
	REGISTER(free);
	REGISTER(attach);
	REGISTER(detach);
	REGISTER(flushVr);
	REGISTER(rule);
	REGISTER(rate);

	if (Tcl_Export(interp, ns, "*", 0) == TCL_ERROR) {
		return TCL_ERROR;
	}

	if (Tcl_PkgProvide(interp, "lsdn", "1.0") == TCL_ERROR) {
		return TCL_ERROR;
	}

	return TCL_OK;
}
