#include <json.h>

#include "private/dump.h"


static struct json_object *jsonify_lsdn_settings(struct lsdn_settings *s)
{
	struct json_object *jobj_settings = json_object_new_object();
	char ip[LSDN_IP_STRING_LEN + 1];

	json_object_object_add(jobj_settings, "settingsName", json_object_new_string(s->name.str));
	json_object_object_add(jobj_settings, "settingsType", json_object_new_string(s->ops->type));
	if (s->ops->get_port)
		json_object_object_add(jobj_settings, "port", json_object_new_int(s->ops->get_port(s)));
	if (s->ops->get_ip) {
		lsdn_ip_t ip_addr = s->ops->get_ip(s);
		lsdn_ip_to_string(&ip_addr, ip);
		struct json_object *jobj_ip = json_object_new_string(ip);
		json_object_object_add(jobj_settings, "ip", jobj_ip);
	}
	// TODO add settings rules

	return jobj_settings;
}

static struct json_object *jsonify_lsdn_phys(struct lsdn_phys *p)
{
	struct json_object *jobj_phys = json_object_new_object();
	char ip[LSDN_IP_STRING_LEN + 1];

	json_object_object_add(jobj_phys, "physName", json_object_new_string(p->name.str));
	if (p->attr_ip) {
		lsdn_ip_to_string(p->attr_ip, ip);
		json_object_object_add(jobj_phys, "attrIp", json_object_new_string(ip));
	}
	if (p->attr_iface)
		json_object_object_add(jobj_phys, "iface", json_object_new_string(p->attr_iface));
	json_object_object_add(jobj_phys, "isLocal", json_object_new_boolean(p->is_local));

	return jobj_phys;
}

static struct json_object *jsonify_lsdn_virt(struct lsdn_virt *virt)
{
	struct json_object *jobj_virt = json_object_new_object();
	char mac_buf[LSDN_MAC_STRING_LEN + 1];

	json_object_object_add(jobj_virt, "virtName", json_object_new_string(virt->name.str));
	if (virt->attr_mac) {
		lsdn_mac_to_string(virt->attr_mac, mac_buf);
		json_object_object_add(jobj_virt, "attrMac", json_object_new_string(mac_buf));
	}
	if (virt->connected_through)
		json_object_object_add(jobj_virt, "phys", json_object_new_string(
			virt->connected_through->phys->name.str));
	if (virt->connected_if.ifname)
		json_object_object_add(jobj_virt, "iface", json_object_new_string(virt->connected_if.ifname));

	// TODO add virt rules

	return jobj_virt;
}

static struct json_object *jsonify_lsdn_net(struct lsdn_net *net)
{
	struct json_object *jobj_net = json_object_new_object();

	json_object_object_add(jobj_net, "netName", json_object_new_string(net->name.str));
	json_object_object_add(jobj_net, "vnetId", json_object_new_int(net->vnet_id));

	struct json_object *jarr_phys_list = json_object_new_array();
	lsdn_foreach(net->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
		struct json_object *jobj_phys = json_object_new_string(pa->phys->name.str);
		json_object_array_add(jarr_phys_list, jobj_phys);
	}
	json_object_object_add(jobj_net, "physList", jarr_phys_list);

	struct json_object *jarr_virts = json_object_new_array();
	lsdn_foreach(net->virt_list, virt_entry, struct lsdn_virt, virt) {
		struct json_object *jobj_virt = jsonify_lsdn_virt(virt);
		json_object_array_add(jarr_virts, jobj_virt);
	}
	json_object_object_add(jobj_net, "virts", jarr_virts);

	return jobj_net;
}

const char *lsdn_dump_context_json(struct lsdn_context *ctx)
{
	// TODO handle OOM errors
	// TODO release json objects
	struct json_object *jobj = json_object_new_object();

	json_object_object_add(jobj, "lsdnName", json_object_new_string(ctx->name));

	struct json_object *jarr_settings = json_object_new_array();
	lsdn_foreach(ctx->settings_list, settings_entry, struct lsdn_settings, s) {
		struct json_object *jobj_settings = jsonify_lsdn_settings(s);
		json_object_array_add(jarr_settings, jobj_settings);
	}
	json_object_object_add(jobj, "lsdnSettings", jarr_settings);

	struct json_object *jarr_physes = json_object_new_array();
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p) {
		struct json_object *jobj_phys = jsonify_lsdn_phys(p);
		json_object_array_add(jarr_physes, jobj_phys);
	}
	json_object_object_add(jobj, "lsdnPhyses", jarr_physes);

	struct json_object *jarr_nets = json_object_new_array();
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, net) {
		struct json_object *jobj_net = jsonify_lsdn_net(net);
		json_object_array_add(jarr_nets, jobj_net);
	}
	json_object_object_add(jobj, "lsdnNets", jarr_nets);

	return json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
}

struct lsdn_context *lsdn_load_context_json(const char *str)
{
	struct lsdn_context *ctx = NULL;
	// TODO
	return ctx;
}
