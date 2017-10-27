#include "lsext.h"
#include "../netmodel/include/lsdn.h"

enum level{L_ROOT, L_NET};
struct tcl_ctx{
	struct lsdn_context *lsctx;
	struct lsdn_net *net;
	enum level level;
};

#define CMD(name) static int tcl_##name(struct tcl_ctx *ctx, Tcl_Interp *interp, int argc, Tcl_Obj *const argv[])

static int tcl_error(Tcl_Interp *interp, const char *err) {
	Tcl_SetResult(interp, (char*) err, NULL);
	return TCL_ERROR;
}

static int store_arg(ClientData d, Tcl_Obj *obj, void *dst_ptr)
{
	Tcl_Obj **dst_typed = (Tcl_Obj**) dst_ptr;
	*dst_typed = obj;
	return 1;
}

static int create_nets(Tcl_Interp *interp, Tcl_Obj *nets, struct lsdn_settings *settings)
{
	if(!nets)
		return tcl_error(interp, "-nets argument is required");

	int netc;
	Tcl_Obj** netv;
	if(Tcl_ListObjGetElements(interp, nets, &netc, &netv) != TCL_OK)
		return TCL_ERROR;
	/* first just validate */
	for(int i = 0; i < netc; i++) {
		int netid;
		if(Tcl_GetIntFromObj(interp, netv[i], &netid) != TCL_OK)
			return TCL_ERROR;
	}

	/* now create */
	for(int i = 0; i < netc; i++) {
		int netid;
		if(Tcl_GetIntFromObj(interp, netv[i], &netid) != TCL_OK)
			return TCL_ERROR;
		struct lsdn_net *net = lsdn_net_new(settings, netid);
		lsdn_net_set_name(net, Tcl_GetString(netv[i]));
	}
	return TCL_OK;
}

CMD(settings_direct)
{
	Tcl_Obj *nets = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_FUNC, "-nets", store_arg, &nets},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_direct(ctx->lsctx);
	return create_nets(interp, nets, settings);
}

CMD(settings_vlan)
{
	Tcl_Obj *nets = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_FUNC, "-nets", store_arg, &nets},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_vlan(ctx->lsctx);
	return create_nets(interp, nets, settings);
}

CMD(settings_vxlan_e2e)
{
	Tcl_Obj *nets = NULL;
	int port = 0;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_FUNC, "-nets", store_arg, &nets},
		{TCL_ARGV_INT, "-port", NULL, &port},
		{TCL_ARGV_END}
	};
	argc--; argv++;
	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_vxlan_e2e(ctx->lsctx, port);
	return create_nets(interp, nets, settings);
}

CMD(settings_vxlan_mcast)
{
	Tcl_Obj *nets = NULL;
	const char* ip;
	lsdn_ip_t ip_parsed;
	int port = 0;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_FUNC, "-nets", store_arg, &nets},
		{TCL_ARGV_STRING, "-mcastIp", NULL, &ip},
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
	return create_nets(interp, nets, settings);
}

CMD(settings_vxlan_static)
{
	Tcl_Obj *nets = NULL;
	int port = 0;
	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_FUNC, "-nets", store_arg, &nets},
		{TCL_ARGV_INT, "-port", NULL, &port},
		{TCL_ARGV_END}
	};
	argc--; argv++;

	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	struct lsdn_settings * settings = lsdn_settings_new_vxlan_static(ctx->lsctx, port);
	return create_nets(interp, nets, settings);
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

	if(ctx->level != L_ROOT)
		tcl_error(interp, "settings command can not be nested");

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
	if(argc != 3) {
		Tcl_WrongNumArgs(interp, 1, argv, "net-id contents");
		return TCL_ERROR;
	}
	if(ctx->level != L_ROOT)
		tcl_error(interp, "net command can not be nested");

	ctx->net = lsdn_net_by_name(ctx->lsctx, Tcl_GetString(argv[1]));
	if(!ctx->net)
		return tcl_error(interp, "net not found");

	ctx->level = L_NET;
	lsdn_net_set_name(ctx->net, Tcl_GetString(argv[1]));
	int r = Tcl_EvalObj(interp, argv[2]);
	ctx->level = L_ROOT;
	return r;
}

CMD(virt)
{
	if(ctx->level != L_NET)
		/* TODO: relax this requirement -- allow passing the net to the command*/
		return tcl_error(interp, "virt command may only appear inside net definition");

	/* TODO: allowing naming of virts */

	const char *mac = NULL;
	lsdn_mac_t mac_parsed;
	const char *phys = NULL;
	struct lsdn_phys *phys_parsed = NULL;
	const char *iface = NULL;
	const char *name = NULL;
	struct lsdn_virt *virt = NULL;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-mac", NULL, &mac},
		{TCL_ARGV_STRING, "-phys", NULL, &phys},
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
	if(phys) {
		phys_parsed = lsdn_phys_by_name(ctx->lsctx, phys);
		if(!phys_parsed)
			return tcl_error(interp, "phys was not found");
	}
	if (!virt)
		virt = lsdn_virt_new(ctx->net);
	if(name)
		lsdn_virt_set_name(virt, name);
	if(mac)
		lsdn_virt_set_mac(virt, mac_parsed);
	if(phys)
		lsdn_virt_connect(virt, phys_parsed, iface);

	return TCL_OK;
}

CMD(phys)
{
	if(ctx->level != L_ROOT)
		tcl_error(interp, "phys command can not be nested");

	const char *name = NULL;
	const char *iface = NULL;
	const char *ip = NULL;
	lsdn_ip_t ip_parsed;
	Tcl_Obj *nets = NULL;
	int netc;
	Tcl_Obj** netv;

	const Tcl_ArgvInfo opts[] = {
		{TCL_ARGV_STRING, "-name", NULL, &name},
		{TCL_ARGV_STRING, "-if", NULL, &iface},
		{TCL_ARGV_STRING, "-ip", NULL, &ip},
		{TCL_ARGV_FUNC, "-nets", store_arg, &nets},
		{TCL_ARGV_END}
	};
	if(Tcl_ParseArgsObjv(interp, opts, &argc, argv, NULL) != TCL_OK)
		return TCL_ERROR;

	if(nets && Tcl_ListObjGetElements(interp, nets, &netc, &netv) != TCL_OK)
		return TCL_ERROR;
	if(ip) {
		if(lsdn_parse_ip(&ip_parsed, ip) != LSDNE_OK)
			return tcl_error(interp, "ip address not in valid format");
	}

	struct lsdn_phys *phys = lsdn_phys_new(ctx->lsctx);
	if(name)
		lsdn_phys_set_name(phys, name);
	if(iface)
		lsdn_phys_set_iface(phys, iface);
	if(ip)
		lsdn_phys_set_ip(phys, ip_parsed);

	if(nets){
		for(int i = 0; i < netc; i++) {
			struct lsdn_net *net = lsdn_net_by_name(ctx->lsctx, Tcl_GetString(netv[i]));
			if(!net)
				return tcl_error(interp, "network not found");
			lsdn_phys_attach(phys, net);
		}
	}

	return TCL_OK;
}

CMD(commit)
{
	if(ctx->level != L_ROOT)
		tcl_error(interp, "commit command can not be nested");

	if(lsdn_commit(ctx->lsctx, lsdn_problem_stderr_handler, NULL) != LSDNE_OK)
		return tcl_error(interp, "commit error");
	return TCL_OK;
}

CMD(validate)
{
	if(ctx->level != L_ROOT)
		tcl_error(interp, "validate command can not be nested");

	if(lsdn_validate(ctx->lsctx, lsdn_problem_stderr_handler, NULL) != LSDNE_OK)
		return tcl_error(interp, "commit error");
	return TCL_OK;
}

CMD(claimLocal)
{
	if(ctx->level != L_ROOT)
		tcl_error(interp, "claimLocal command can not be nested");
	if(argc != 2) {
		Tcl_WrongNumArgs(interp, argc, argv, "phys");
		return TCL_ERROR;
	}

	struct lsdn_phys *phys = lsdn_phys_by_name(ctx->lsctx, Tcl_GetString(argv[1]));
	if(!phys)
		return tcl_error(interp, "phys not found");
	lsdn_phys_claim_local(phys);
	return TCL_OK;
}

static struct tcl_ctx default_ctx;

#define REGISTER(name) Tcl_CreateObjCommand(interp, #name, (Tcl_ObjCmdProc*) tcl_##name, ctx, NULL)

int register_lsdn_tcl(Tcl_Interp *interp)
{
	struct tcl_ctx *ctx = &default_ctx;
	ctx->lsctx = lsdn_context_new("lsdn");
	ctx->level = L_ROOT;
	lsdn_context_abort_on_nomem(ctx->lsctx);
	REGISTER(settings);
	REGISTER(net);
	REGISTER(virt);
	REGISTER(phys);
	REGISTER(commit);
	REGISTER(validate);
	REGISTER(claimLocal);

	return TCL_OK;
}
