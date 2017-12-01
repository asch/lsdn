/** \file
 * TCL extensions for lsctl language. */

#include "lsext.h"
#include <stdlib.h>
#include "../netmodel/include/lsdn.h"

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
		return tcl_error(interp, "-net argument is not allowed inside net scope");
	} else if (ctx->phys) {
		*phys = ctx->phys;
	} else if (arg) {
		*phys = lsdn_phys_by_name(ctx->lsctx, arg);
		if (!*phys)
			return tcl_error(interp, "can not find network");
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
	int port = 0;

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
	int port = 0;

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
	int port = 0;
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

CMD(settings)
{
	int type;
	static const char *type_names[] = {"direct", "vlan", "vxlan/mcast", "vxlan/static", "vxlan/e2e", NULL};
	enum types {T_DIRECT, T_VLAN, T_VXLAN_MCAST, T_VXLAN_STATIC, T_VXLAN_E2E};

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
	Tcl_Obj **pos_args = NULL;
	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_INT, "-vid", NULL, &vnet_id},
		{TCL_ARGV_STRING, "-settings", NULL, &settings_name},
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
		{TCL_ARGV_END}
	};

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args) != TCL_OK)
		return TCL_ERROR;

	if(resolve_phys_arg(interp, ctx, phys, &phys_parsed, false))
		return TCL_ERROR;

	if(argc != 3) {
		Tcl_WrongNumArgs(interp, 1, argv, "name contents");
		ckfree(pos_args);
		return TCL_ERROR;
	}

	if (!vnet_id)
		vnet_id = strtol(Tcl_GetString(pos_args[1]), NULL, 10);
	struct lsdn_net *net = lsdn_net_by_name(ctx->lsctx, Tcl_GetString(pos_args[1]));
	if(!net) {
		struct lsdn_settings *s;
		if (!settings_name) {
			s = lsdn_settings_by_name(ctx->lsctx, "default");
			if (!s)
				return tcl_error(interp, "Please define default network settings first");
		} else {
			s = lsdn_settings_by_name(ctx->lsctx, settings_name);
			if (!s)
				return tcl_error(interp, "settings not found");
		}

		net = lsdn_net_new(s, vnet_id);
	} else {
		if (settings_name)
			return tcl_error(interp, "Can not change settings, the network already exists");
	}

	if(phys_parsed)
		lsdn_phys_attach(phys_parsed, net);

	lsdn_net_set_name(net, Tcl_GetString(pos_args[1]));
	push_scope(ctx, S_NET);
	ctx->net = net;
	int r = Tcl_EvalObj(interp, pos_args[2]);
	ctx->net = NULL;
	pop_scope(ctx);

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
	struct lsdn_virt *virt = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-mac", NULL, &mac},
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
		{TCL_ARGV_STRING, "-net", NULL, &net},
		{TCL_ARGV_STRING, "-if", NULL, &iface},
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_END}
	};

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	if(name) {
		virt = lsdn_virt_by_name(ctx->net, name);
	}

	if(mac) {
		if(lsdn_parse_mac(&mac_parsed, mac) != LSDNE_OK)
			return tcl_error(interp, "mac address is in invalid format");
	}

	if(resolve_net_arg(interp, ctx, net, &net_parsed, true))
		return TCL_ERROR;

	if(resolve_phys_arg(interp, ctx, phys, &phys_parsed, false))
		return TCL_ERROR;

	if (!virt)
		virt = lsdn_virt_new(net_parsed);
	if(name)
		lsdn_virt_set_name(virt, name);
	if(mac)
		lsdn_virt_set_mac(virt, mac_parsed);
	if(phys_parsed)
		lsdn_virt_connect(virt, phys_parsed, iface);

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
	lsdn_ip_t ip_parsed;
	Tcl_Obj **pos_args = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_STRING, "-if", NULL, &iface},
		{TCL_ARGV_STRING, "-ip", NULL, &ip},
		{TCL_ARGV_STRING, "-net", NULL, &net},
		{TCL_ARGV_END}
	};
	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, &pos_args))
		return TCL_ERROR;

	if(resolve_net_arg(interp, ctx, net, &net_parsed, false)) {
		ckfree(pos_args);
		return TCL_ERROR;
	}

	if(ip) {
		if(lsdn_parse_ip(&ip_parsed, ip) != LSDNE_OK) {
			ckfree(pos_args);
			return tcl_error(interp, "ip address not in valid format");
		}
	}

	struct lsdn_phys *phys = NULL;
	if (name)
		phys = lsdn_phys_by_name(ctx->lsctx, name);

	if (!phys)
		phys = lsdn_phys_new(ctx->lsctx);
	if(name)
		lsdn_phys_set_name(phys, name);
	if(iface)
		lsdn_phys_set_iface(phys, iface);
	if(ip)
		lsdn_phys_set_ip(phys, ip_parsed);
	if(net_parsed)
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

#define REGISTER(name) Tcl_CreateObjCommand(interp, #name, (Tcl_ObjCmdProc*) tcl_##name, ctx, NULL)

int register_lsdn_tcl(Tcl_Interp *interp)
{
	struct tcl_ctx *ctx = &default_ctx;
	ctx->lsctx = lsdn_context_new("lsdn");
	ctx->stack_pos = 0;
	ctx->scope_stack[0] = S_ROOT;
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

	return TCL_OK;
}
