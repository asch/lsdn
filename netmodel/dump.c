#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <json.h>

#include "include/dump.h"
#include "private/net.h"
#include "include/nettypes.h"


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

static struct json_object *jsonify_lsdn_qos_rate(lsdn_qos_rate_t *qos)
{
	// TODO unify double / float / uint32 types in lsctl, netmodel and dump
	struct json_object *jqos = json_object_new_object();
	json_object_object_add(jqos, "avgRate", json_object_new_double(qos->avg_rate));
	json_object_object_add(jqos, "burstSize", json_object_new_int64(qos->burst_size));
	json_object_object_add(jqos, "burstRate", json_object_new_double(qos->burst_rate));
	return jqos;
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

	if (virt->attr_rate_in) {
		struct json_object *jvirt_qos_in = jsonify_lsdn_qos_rate(virt->attr_rate_in);
		json_object_object_add(jobj_virt, "qosIn", jvirt_qos_in);
	}
	if (virt->attr_rate_out) {
		struct json_object *jvirt_qos_out = jsonify_lsdn_qos_rate(virt->attr_rate_out);
		json_object_object_add(jobj_virt, "qosOut", jvirt_qos_out);
	}

	return jobj_virt;
}

static struct json_object *jsonify_lsdn_net(struct lsdn_net *net)
{
	struct json_object *jobj_net = json_object_new_object();

	json_object_object_add(jobj_net, "netName", json_object_new_string(net->name.str));
	json_object_object_add(jobj_net, "settings", json_object_new_string(net->settings->name.str));
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

#define NEWLINE "\n"
#define SPACE " "

struct strbuf {
	char *s;
	char *end;
	size_t len;
};

static struct strbuf *strbuf_create(size_t hint)
{
	size_t size = hint ? hint : 128;
	struct strbuf *sbuf = (struct strbuf *) malloc(sizeof(*sbuf));
	if (!sbuf)
		return NULL;
	sbuf->s = (char *) malloc(size);
	if (!sbuf->s) {
		free(sbuf);
		return NULL;
	}
	sbuf->s[0] = '\0';
	sbuf->end = sbuf->s;
	sbuf->len = size;
	return sbuf;
}

static struct strbuf *strbuf_append_str(
		struct strbuf *sbuf, const char *str)
{
	size_t size = sbuf->end - sbuf->s;
	size_t free_space = sbuf->len - size - 1;
	size_t str_len = strlen(str);
	if (free_space < str_len) {
		size_t new_size = sbuf->len > str_len ?
			(sbuf->len * 2) : (str_len * 2);
		char *s = (char *) malloc(new_size);
		if (!s)
			abort();
		strcpy(s, sbuf->s);
		free(sbuf->s);
		sbuf->s = s;
		sbuf->len = new_size;
		sbuf->end = sbuf->s + size;
	}
	strcpy(sbuf->end, str);
	sbuf->end += str_len;
	return sbuf;
}

static struct strbuf *strbuf_append(struct strbuf *sbuf, ...)
{
	va_list args;
	const char *str;

	va_start(args, sbuf);
	while ((str = va_arg(args, char *)))
		sbuf = strbuf_append_str(sbuf, str);
	va_end(args);

	return sbuf;
}

static inline char *strbuf_to_str(struct strbuf *sbuf)
{
	return strdup(sbuf->s);
}

static inline void strbuf_free(struct strbuf *sbuf)
{
	free(sbuf->s);
	free(sbuf);
}

static void parse_json_lsdn_settings(struct json_object *jobj, struct strbuf *sbuf)
{
	if (!json_object_is_type(jobj, json_type_array))
		abort();
	int len = json_object_array_length(jobj);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *jsettings = json_object_array_get_idx(jobj, idx);
		if (!json_object_is_type(jsettings, json_type_object))
			abort();
		strbuf_append(sbuf, "settings", SPACE, NULL);
		struct json_object *val;
		if (!json_object_object_get_ex(jsettings, "settingsType", &val))
			abort();
		strbuf_append(sbuf, json_object_get_string(val), SPACE, NULL);

		if (json_object_object_get_ex(jsettings, "settingsName", &val)) {
			strbuf_append(sbuf, "-name", SPACE, json_object_get_string(val), SPACE, NULL);
		}
		if (json_object_object_get_ex(jsettings, "port", &val)) {
			strbuf_append(sbuf, "-port", SPACE, json_object_get_string(val), SPACE, NULL);
		}
		if (json_object_object_get_ex(jsettings, "ip", &val)) {
			strbuf_append(sbuf, "-mcastIp", SPACE, json_object_get_string(val), NULL);
		}
		strbuf_append(sbuf, NEWLINE, NULL);
	}
}

static char *target_match_conversion(const char *target)
{
	static char *LOOKUP_TABLE[][2] = {
		{ "src_mac", "-srcMac" },
		{ "dst_mac", "-dstMac" },
		{ "src_ipv4", "-srcIp" },
		{ "dst_ipv4", "-dstIp" },
		{ "src_ipv6", "-srcIp" },
		{ "dst_ipv6", "-dstIp" }
	};
	for (size_t i = 0; i < 6; i++)
		if (!strcmp(target, LOOKUP_TABLE[i][0]))
			return LOOKUP_TABLE[i][1];
	return NULL;
}

static bool target_has_mask(const char *target)
{
	static char *HAS_MASK_TABLE[] = {
		"src_ipv4",
		"dst_ipv4",
		"src_ipv6",
		"dst_ipv6"
	};
	for (size_t i = 0; i < 4; i++)
		if (!strcmp(target, HAS_MASK_TABLE[i]))
			return true;
	return false;
}

static void convert_rules(struct json_object *rules, struct strbuf *sbuf)
{
	char ip_pref[8 + 1];

	if (!json_object_is_type(rules, json_type_array))
		abort();
	int len = json_object_array_length(rules);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *rule = json_object_array_get_idx(rules, idx);
		if (!json_object_is_type(rule, json_type_object))
			abort();
		strbuf_append(sbuf, "rule", SPACE, NULL);
		struct json_object *val;
		if (!json_object_object_get_ex(rule, "dir", &val))
			abort();
		strbuf_append(sbuf, json_object_get_string(val), SPACE, NULL);
		if (!json_object_object_get_ex(rule, "prio", &val))
			abort();
		strbuf_append(sbuf, json_object_get_string(val), SPACE, NULL);
		if (!json_object_object_get_ex(rule, "action", &val))
			abort();
		strbuf_append(sbuf, json_object_get_string(val), SPACE, NULL);

		if (json_object_object_get_ex(rule, "targets", &val)) {
			if (!json_object_is_type(val, json_type_array))
				abort();
			int targets_len = json_object_array_length(val);
			for (int idx = 0; idx < targets_len; idx++) {
				struct json_object *wal = json_object_array_get_idx(val, idx);

				struct json_object *zal;
				if (!json_object_object_get_ex(wal, "target", &zal))
					abort();
				const char *target = json_object_get_string(zal);
				strbuf_append(sbuf, target_match_conversion(target), SPACE, NULL);
				if (!json_object_object_get_ex(wal, "match", &zal))
					abort();
				strbuf_append(sbuf, json_object_get_string(zal), NULL);
				if (target_has_mask(target)) {
					if (json_object_object_get_ex(wal, "matchMask", &zal)) {
						lsdn_ip_t ip;
						lsdn_err_t res = lsdn_parse_ip(&ip, json_object_get_string(zal));
						if (res != LSDNE_OK)
							abort();
						int prefix = lsdn_ip_prefix_from_mask(&ip);
						sprintf(ip_pref, "%d", prefix);
						strbuf_append(sbuf, "/", ip_pref, NULL);
					}
				}
			}
		}
		strbuf_append(sbuf, NEWLINE, NULL);
	}
}

static void convert_qos(struct json_object *qos, bool in, struct strbuf *sbuf)
{
	if (!json_object_is_type(qos, json_type_object))
		abort();
	strbuf_append(sbuf, "rate", SPACE, NULL);
	if (in)
		strbuf_append(sbuf, "in", SPACE, NULL);
	else
		strbuf_append(sbuf, "out", SPACE, NULL);

	struct json_object *attr;
	if (json_object_object_get_ex(qos, "avgRate", &attr))
		strbuf_append(sbuf, "-avg", SPACE, json_object_get_string(attr), SPACE, NULL);
	if (json_object_object_get_ex(qos, "burstSize", &attr))
		strbuf_append(sbuf, "-burst", SPACE, json_object_get_string(attr), SPACE, NULL);
	if (json_object_object_get_ex(qos, "burstRate", &attr))
		strbuf_append(sbuf, "-burstRate", SPACE, json_object_get_string(attr), SPACE, NULL);

	strbuf_append(sbuf, NEWLINE, NULL);
}

static void parse_json_lsdn_nets(struct json_object *jobj, struct strbuf *sbuf)
{
	if (!json_object_is_type(jobj, json_type_array))
		abort();
	int len = json_object_array_length(jobj);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *jnet = json_object_array_get_idx(jobj, idx);
		if (!json_object_is_type(jnet, json_type_object))
			abort();
		strbuf_append(sbuf, "net", SPACE, NULL);
		struct json_object *val;
		if (json_object_object_get_ex(jnet, "vnetId", &val))
			strbuf_append(sbuf, "-vid", SPACE, json_object_get_string(val), SPACE, NULL);
		if (json_object_object_get_ex(jnet, "settings", &val))
			strbuf_append(sbuf, "-settings", SPACE, json_object_get_string(val), SPACE, NULL);
		if (json_object_object_get_ex(jnet, "netName", &val))
			strbuf_append(sbuf, json_object_get_string(val), SPACE, NULL);
		strbuf_append(sbuf, "{", NEWLINE, NULL);

		struct json_object *phys_list;
		if (json_object_object_get_ex(jnet, "physList", &phys_list)) {
			if (!json_object_is_type(phys_list, json_type_array))
				abort();
			int len_phys_list = json_object_array_length(phys_list);
			for (int idx = 0; idx < len_phys_list; idx++) {
				struct json_object *p = json_object_array_get_idx(phys_list, idx);
				strbuf_append(sbuf, "attach", SPACE, json_object_get_string(p), NEWLINE, NULL);
			}
		}

		struct json_object *virt_list;
		if (json_object_object_get_ex(jnet, "virts", &virt_list)) {
			if (!json_object_is_type(virt_list, json_type_array))
				abort();
			int len_virt_list = json_object_array_length(virt_list);
			for (int idx = 0; idx < len_virt_list; idx++) {
				struct json_object *p = json_object_array_get_idx(virt_list, idx);
				strbuf_append(sbuf, "virt", SPACE, NULL);
				struct json_object *rules = NULL;
				struct json_object *qos_in = NULL;
				struct json_object *qos_out = NULL;
				json_object_object_foreach(p, pkey, pval) {
					if (!strcmp(pkey, "virtName"))
						strbuf_append(sbuf, "-name", SPACE, json_object_get_string(pval), SPACE, NULL);
					else if (!strcmp(pkey, "phys"))
						strbuf_append(sbuf, "-phys", SPACE, json_object_get_string(pval), SPACE, NULL);
					else if (!strcmp(pkey, "iface"))
						strbuf_append(sbuf, "-if", SPACE, json_object_get_string(pval), SPACE, NULL);
					else if (!strcmp(pkey, "attrMac"))
						strbuf_append(sbuf, "-mac", SPACE, json_object_get_string(pval), SPACE, NULL);
					else if (!strcmp(pkey, "qosIn"))
						qos_in = pval;
					else if (!strcmp(pkey, "qosOut"))
						qos_out = pval;
					else if (!strcmp(pkey, "rules"))
						rules = pval;
				}
				strbuf_append(sbuf, "{", NEWLINE, NULL);
				if (rules)
					convert_rules(rules, sbuf);
				if (qos_in)
					convert_qos(qos_in, true, sbuf);
				if (qos_out)
					convert_qos(qos_out, false, sbuf);
				strbuf_append(sbuf, "}", NEWLINE, NULL);
			}
		}
		strbuf_append(sbuf, "}", NEWLINE, NULL);
	}
}

static void parse_json_lsdn_physes(struct json_object *jobj, struct strbuf *sbuf)
{
	if (!json_object_is_type(jobj, json_type_array))
		abort();
	int len = json_object_array_length(jobj);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *jphys = json_object_array_get_idx(jobj, idx);
		if (!json_object_is_type(jphys, json_type_object))
			abort();
		strbuf_append(sbuf, "phys", SPACE, NULL);
		char *name = NULL;
		struct json_object *val;
		if (json_object_object_get_ex(jphys, "physName", &val)) {
			strbuf_append(sbuf, "-name", SPACE, NULL);
			name = strdup(json_object_get_string(val));
			if (!name)
				abort();
			strbuf_append(sbuf, name, SPACE, NULL);
		}
		if (json_object_object_get_ex(jphys, "attrIp", &val))
			strbuf_append(sbuf, "-ip", SPACE, json_object_get_string(val), SPACE, NULL);
		if (json_object_object_get_ex(jphys, "iface", &val))
			strbuf_append(sbuf, "-if", SPACE, json_object_get_string(val), SPACE, NULL);
		strbuf_append(sbuf, NEWLINE, NULL);
		if (json_object_object_get_ex(jphys, "isLocal", &val) && name)
			strbuf_append(sbuf, "claimLocal", SPACE, name, NULL);
		strbuf_append(sbuf, NEWLINE, NULL);
		free(name);
	}
}

static void parse_json_lsdn_name(struct json_object *jobj, struct strbuf *sbuf)
{
	const char *name = json_object_get_string(jobj);
	strbuf_append(sbuf, "-name", SPACE, name, NEWLINE, NULL);
}

char *lsdn_convert_context_json2tcl(const char *str_json)
{
	// TODO handle OOM
	struct strbuf *sbuf = strbuf_create(0);
	if (!sbuf)
		abort();

	struct json_object *jobj = json_tokener_parse(str_json);
	if (!json_object_is_type(jobj, json_type_object))
		abort();
	json_object_object_foreach(jobj, key, val) {
		if (!strcmp(key, "lsdnName")) {
			// TODO context name is not in the current lsctl
			//parse_json_lsdn_name(val, sbuf);
		} else if (!strcmp(key, "lsdnSettings")) {
			parse_json_lsdn_settings(val, sbuf);
		} else if (!strcmp(key, "lsdnPhyses")) {
			parse_json_lsdn_physes(val, sbuf);
		} else if (!strcmp(key, "lsdnNets")) {
			parse_json_lsdn_nets(val, sbuf);
		}
	}
	json_object_put(jobj);

	char *res = strbuf_to_str(sbuf);
	if (!res)
		abort();
	strbuf_free(sbuf);

	return res;
}

#undef NEWLINE
#undef SPACE
