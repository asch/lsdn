#include <string.h>
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

static struct json_object *jsonify_lsdn_vr(struct lsdn_vr *vr)
{
	struct json_object *jvr_obj = json_object_new_object();
	char mac[LSDN_MAC_STRING_LEN + 1];
	char ip[LSDN_IP_STRING_LEN + 1];

	struct json_object *jvr_arr = json_object_new_array();
	for (size_t i = 0; i < vr->pos; i++) {
		struct json_object *jvr = json_object_new_object();
		const char *target = lsdn_rule_target_name(vr->targets[i]);
		json_object_object_add(jvr, "target", json_object_new_string(target));

		switch (vr->targets[i]) {
		case LSDN_MATCH_SRC_MAC:
		case LSDN_MATCH_DST_MAC:
			lsdn_mac_to_string(&vr->rule.matches[i].mac, mac);
			json_object_object_add(jvr, "match", json_object_new_string(mac));
			lsdn_mac_to_string(&vr->masks[i].mac, mac);
			json_object_object_add(jvr, "matchMask", json_object_new_string(mac));
			break;
		case LSDN_MATCH_SRC_IPV4:
		case LSDN_MATCH_DST_IPV4:
			lsdn_ipv4_to_string(&vr->rule.matches[i].ipv4, ip);
			json_object_object_add(jvr, "match", json_object_new_string(ip));
			lsdn_ipv4_to_string(&vr->masks[i].ipv4, ip);
			json_object_object_add(jvr, "matchMask", json_object_new_string(ip));
			break;
		case LSDN_MATCH_SRC_IPV6:
		case LSDN_MATCH_DST_IPV6:
			lsdn_ipv6_to_string(&vr->rule.matches[i].ipv6, ip);
			json_object_object_add(jvr, "match", json_object_new_string(ip));
			lsdn_ipv6_to_string(&vr->masks[i].ipv6, ip);
			json_object_object_add(jvr, "matchMask", json_object_new_string(ip));
			break;
		default:
			break;
		}
		json_object_array_add(jvr_arr, jvr);
	}
	json_object_object_add(jvr_obj, "targets", jvr_arr);

	return jvr_obj;
}

static struct json_object *jsonify_lsdn_virt_rules(struct lsdn_virt *virt)
{
	struct json_object *jvr_arr = json_object_new_array();

	struct vr_prio *prio, *tmp;
	HASH_ITER(hh, virt->ht_in_rules, prio, tmp) {
		lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, vr) {
			struct json_object *jvr_obj = jsonify_lsdn_vr(vr);
			json_object_object_add(jvr_obj, "dir", json_object_new_string("in"));
			json_object_object_add(jvr_obj, "prio", json_object_new_int(prio->prio_num));
			json_object_object_add(jvr_obj, "action", json_object_new_string(vr->rule.action.name));
			json_object_array_add(jvr_arr, jvr_obj);
		}
	}
	HASH_ITER(hh, virt->ht_out_rules, prio, tmp) {
		lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, vr) {
			struct json_object *jvr_obj = jsonify_lsdn_vr(vr);
			json_object_object_add(jvr_obj, "dir", json_object_new_string("out"));
			json_object_object_add(jvr_obj, "prio", json_object_new_int(prio->prio_num));
			json_object_object_add(jvr_obj, "action", json_object_new_string(vr->rule.action.name));
			json_object_array_add(jvr_arr, jvr_obj);
		}
	}

	return jvr_arr;
}

static struct json_object *jsonify_lsdn_virt(struct lsdn_virt *virt)
{
	struct json_object *jobj_virt = json_object_new_object();
	char mac[LSDN_MAC_STRING_LEN + 1];

	json_object_object_add(jobj_virt, "virtName", json_object_new_string(virt->name.str));
	if (virt->attr_mac) {
		lsdn_mac_to_string(virt->attr_mac, mac);
		json_object_object_add(jobj_virt, "attrMac", json_object_new_string(mac));
	}
	if (virt->connected_through)
		json_object_object_add(jobj_virt, "phys", json_object_new_string(
					virt->connected_through->phys->name.str));
	if (virt->connected_if.ifname)
		json_object_object_add(jobj_virt, "iface", json_object_new_string(virt->connected_if.ifname));

	struct json_object *jvirt_rules_arr = jsonify_lsdn_virt_rules(virt);
	json_object_object_add(jobj_virt, "rules", jvirt_rules_arr);

	return jobj_virt;
}

static struct json_object *jsonify_lsdn_net(struct lsdn_net *net)
{
	struct json_object *jobj_net = json_object_new_object();

	json_object_object_add(jobj_net, "netName", json_object_new_string(net->name.str));
	json_object_object_add(jobj_net, "vnetId", json_object_new_int64(net->vnet_id));

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

char *lsdn_dump_context_json(struct lsdn_context *ctx)
{
	// TODO handle OOM errors
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

	const char *str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
	char *dup = strdup(str);

	json_object_put(jobj);

	return dup;
}

struct lsdn_context *lsdn_load_context_json(const char *str)
{
	struct lsdn_context *ctx = NULL;
	// TODO
	return ctx;
}
