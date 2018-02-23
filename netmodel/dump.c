#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <json.h>

#include "include/dump.h"
#include "private/net.h"
#include "private/errors.h"
#include "include/nettypes.h"


static struct json_object *jsonify_lsdn_settings(struct lsdn_settings *s)
{
	char ip[LSDN_IP_STRING_LEN + 1];
	struct json_object *jobj_settings = json_object_new_object();
	if (!jobj_settings)
		goto err;

	struct json_object *jstr = json_object_new_string(s->name.str);
	if (!jstr)
		goto err;
	json_object_object_add(jobj_settings, "settingsName", jstr);
	jstr = json_object_new_string(s->ops->type);
	if (!jstr)
		goto err;
	json_object_object_add(jobj_settings, "settingsType", jstr);
	if (s->ops->get_port) {
		struct json_object *jint = json_object_new_int(s->ops->get_port(s));
		if (!jint)
			goto err;
		json_object_object_add(jobj_settings, "port", jint);
	}
	if (s->ops->get_ip) {
		lsdn_ip_t ip_addr = s->ops->get_ip(s);
		lsdn_ip_to_string(&ip_addr, ip);
		struct json_object *jobj_ip = json_object_new_string(ip);
		if (!jobj_ip)
			goto err;
		json_object_object_add(jobj_settings, "ip", jobj_ip);
	}
	return jobj_settings;
err:
	json_object_put(jobj_settings);
	return NULL;
}

static struct json_object *jsonify_lsdn_phys(struct lsdn_phys *p)
{
	char ip[LSDN_IP_STRING_LEN + 1];
	struct json_object *jobj_phys = json_object_new_object();
	if (!jobj_phys)
		goto err;

	struct json_object *jstr = json_object_new_string(p->name.str);
	if (!jstr)
		goto err;
	json_object_object_add(jobj_phys, "physName", jstr);
	if (p->attr_ip) {
		lsdn_ip_to_string(p->attr_ip, ip);
		jstr = json_object_new_string(ip);
		if (!jstr)
			goto err;
		json_object_object_add(jobj_phys, "attrIp", jstr);
	}
	if (p->attr_iface) {
		jstr = json_object_new_string(p->attr_iface);
		if (!jstr)
			goto err;
		json_object_object_add(jobj_phys, "iface", jstr);
	}
	struct json_object *jbool = json_object_new_boolean(p->is_local);
	if (!jbool)
		goto err;
	json_object_object_add(jobj_phys, "isLocal", jbool);

	return jobj_phys;
err:
	json_object_put(jobj_phys);
	return NULL;
}

static struct json_object *jsonify_lsdn_vr(struct lsdn_vr *vr)
{
	char mac[LSDN_MAC_STRING_LEN + 1];
	char ip[LSDN_IP_STRING_LEN + 1];
	struct json_object *jvr_obj = json_object_new_object();
	if (!jvr_obj)
		goto err;

	struct json_object *jvr_arr = json_object_new_array();
	if (!jvr_arr)
		goto err;
	json_object_object_add(jvr_obj, "targets", jvr_arr);
	for (size_t i = 0; i < vr->pos; i++) {
		struct json_object *jvr = json_object_new_object();
		if (!jvr)
			goto err;
		if (json_object_array_add(jvr_arr, jvr)) {
			json_object_put(jvr);
			goto err;
		}
		const char *target = lsdn_rule_target_name(vr->targets[i]);
		struct json_object *jstr = json_object_new_string(target);
		if (!jstr)
			goto err;
		json_object_object_add(jvr, "target", jstr);

		switch (vr->targets[i]) {
		case LSDN_MATCH_SRC_MAC:
		case LSDN_MATCH_DST_MAC:
			lsdn_mac_to_string(&vr->rule.matches[i].mac, mac);
			jstr = json_object_new_string(mac);
			if (!jstr)
				goto err;
			json_object_object_add(jvr, "match", jstr);
			lsdn_mac_to_string(&vr->masks[i].mac, mac);
			jstr = json_object_new_string(mac);
			if (!jstr)
				goto err;
			json_object_object_add(jvr, "matchMask", jstr);
			break;
		case LSDN_MATCH_SRC_IPV4:
		case LSDN_MATCH_DST_IPV4:
			lsdn_ipv4_to_string(&vr->rule.matches[i].ipv4, ip);
			jstr = json_object_new_string(ip);
			if (!jstr)
				goto err;
			json_object_object_add(jvr, "match", jstr);
			lsdn_ipv4_to_string(&vr->masks[i].ipv4, ip);
			jstr = json_object_new_string(ip);
			json_object_object_add(jvr, "matchMask", jstr);
			break;
		case LSDN_MATCH_SRC_IPV6:
		case LSDN_MATCH_DST_IPV6:
			lsdn_ipv6_to_string(&vr->rule.matches[i].ipv6, ip);
			if ((jstr = json_object_new_string(ip)) == NULL)
				goto err;
			json_object_object_add(jvr, "match", jstr);
			lsdn_ipv6_to_string(&vr->masks[i].ipv6, ip);
			if ((jstr = json_object_new_string(ip)) == NULL)
				goto err;
			json_object_object_add(jvr, "matchMask", jstr);
			break;
		default:
			break;
		}
	}

	return jvr_obj;
err:
	json_object_put(jvr_obj);
	return NULL;
}

static struct json_object *jsonify_lsdn_virt_rules(struct lsdn_virt *virt)
{
	struct json_object *jvr_arr = json_object_new_array();
	if (!jvr_arr)
		goto err;

	struct vr_prio *prio, *tmp;
	struct vr_prio *rules = virt->ht_in_rules;
	char *dir = "in";
	struct json_object *jstr, *jint;
	for (int i = 0; i < 2; i++, dir = "out", rules = virt->ht_out_rules) {
		HASH_ITER(hh, rules, prio, tmp) {
			lsdn_foreach(prio->rules_list, rules_entry, struct lsdn_vr, vr) {
				struct json_object *jvr_obj = jsonify_lsdn_vr(vr);
				if (!jvr_obj)
					goto err;
				if (json_object_array_add(jvr_arr, jvr_obj)) {
					json_object_put(jvr_obj);
					goto err;
				}
				if ((jstr = json_object_new_string(dir)) == NULL)
					goto err;
				json_object_object_add(jvr_obj, "dir", jstr);
				jint = json_object_new_int(prio->prio_num);
				if (!jint)
					goto err;
				json_object_object_add(jvr_obj, "prio", jint);
				jstr = json_object_new_string(vr->rule.action.name);
				if (!jstr)
					goto err;
				json_object_object_add(jvr_obj, "action", jstr);
			}
		}
	}

	return jvr_arr;
err:
	json_object_put(jvr_arr);
	return NULL;
}

static struct json_object *jsonify_lsdn_qos_rate(lsdn_qos_rate_t *qos)
{
	struct json_object *jint, *jdouble;
	struct json_object *jqos = json_object_new_object();
	if (!jqos)
		goto err;
	if ((jdouble = json_object_new_double(qos->avg_rate)) == NULL)
		goto err;
	json_object_object_add(jqos, "avgRate", jdouble);
	if ((jint = json_object_new_int64(qos->burst_size)) == NULL)
		goto err;
	json_object_object_add(jqos, "burstSize", jint);
	if ((jdouble = json_object_new_double(qos->burst_rate)) == NULL)
		goto err;
	json_object_object_add(jqos, "burstRate", jdouble);
	return jqos;
err:
	json_object_put(jqos);
	return NULL;
}

static struct json_object *jsonify_lsdn_virt(struct lsdn_virt *virt)
{
	char mac[LSDN_MAC_STRING_LEN + 1];
	struct json_object *jobj_virt = json_object_new_object();
	if (!jobj_virt)
		goto err;

	struct json_object *jstr;
	if ((jstr = json_object_new_string(virt->name.str)) == NULL)
		goto err;
	json_object_object_add(jobj_virt, "virtName", jstr);
	if (virt->attr_mac) {
		lsdn_mac_to_string(virt->attr_mac, mac);
		if ((jstr = json_object_new_string(mac)) == NULL)
			goto err;
		json_object_object_add(jobj_virt, "attrMac", jstr);
	}
	if (virt->connected_through) {
		if ((jstr = json_object_new_string(virt->connected_through->phys->name.str)) == NULL)
			goto err;
		json_object_object_add(jobj_virt, "phys", jstr);
	}
	if (virt->connected_if.ifname) {
		if ((jstr = json_object_new_string(virt->connected_if.ifname)) == NULL)
			goto err;
		json_object_object_add(jobj_virt, "iface", jstr);
	}

	struct json_object *jvirt_rules_arr = jsonify_lsdn_virt_rules(virt);
	if (!jvirt_rules_arr)
		goto err;
	json_object_object_add(jobj_virt, "rules", jvirt_rules_arr);

	if (virt->attr_rate_in) {
		struct json_object *jvirt_qos_in = jsonify_lsdn_qos_rate(virt->attr_rate_in);
		if (!jvirt_qos_in)
			goto err;
		json_object_object_add(jobj_virt, "qosIn", jvirt_qos_in);
	}
	if (virt->attr_rate_out) {
		struct json_object *jvirt_qos_out = jsonify_lsdn_qos_rate(virt->attr_rate_out);
		if (!jvirt_qos_out)
			goto err;
		json_object_object_add(jobj_virt, "qosOut", jvirt_qos_out);
	}

	return jobj_virt;
err:
	json_object_put(jobj_virt);
	return NULL;
}

static struct json_object *jsonify_lsdn_net(struct lsdn_net *net)
{
	struct json_object *jobj_net = json_object_new_object();
	if (!jobj_net)
		goto err;

	struct json_object *jstr = json_object_new_string(net->name.str);
	if (!jstr)
		goto err;
	json_object_object_add(jobj_net, "netName", jstr);
	if ((jstr = json_object_new_string(net->settings->name.str)) == NULL)
		goto err;
	json_object_object_add(jobj_net, "settings", jstr);
	struct json_object *jint = json_object_new_int64(net->vnet_id);
	if (!jint)
		goto err;
	json_object_object_add(jobj_net, "vnetId", jint);

	struct json_object *jarr_phys_list = json_object_new_array();
	if (!jarr_phys_list)
		goto err;
	json_object_object_add(jobj_net, "physList", jarr_phys_list);
	lsdn_foreach(net->attached_list, attached_entry, struct lsdn_phys_attachment, pa) {
		struct json_object *jobj_phys = json_object_new_string(pa->phys->name.str);
		if (!jobj_phys)
			goto err;
		if (json_object_array_add(jarr_phys_list, jobj_phys)) {
			json_object_put(jobj_phys);
			goto err;
		}
	}

	struct json_object *jarr_virts = json_object_new_array();
	if (!jarr_virts)
		goto err;
	json_object_object_add(jobj_net, "virts", jarr_virts);
	lsdn_foreach(net->virt_list, virt_entry, struct lsdn_virt, virt) {
		struct json_object *jobj_virt = jsonify_lsdn_virt(virt);
		if (!jobj_virt)
			goto err;
		if (json_object_array_add(jarr_virts, jobj_virt)) {
			json_object_put(jobj_virt);
			goto err;
		}
	}

	return jobj_net;
err:
	json_object_put(jobj_net);
	return NULL;
}

char *lsdn_dump_context_json(struct lsdn_context *ctx)
{
	char *res = NULL;
	struct json_object *jobj = json_object_new_object();
	if (!jobj)
		goto err;

	struct json_object *jname = json_object_new_string(ctx->name);
	if (!jname)
		goto err;
	json_object_object_add(jobj, "lsdnName", jname);

	struct json_object *jarr_settings = json_object_new_array();
	if (!jarr_settings)
		goto err;
	json_object_object_add(jobj, "lsdnSettings", jarr_settings);
	lsdn_foreach(ctx->settings_list, settings_entry, struct lsdn_settings, s) {
		struct json_object *jobj_settings = jsonify_lsdn_settings(s);
		if (!jobj_settings)
			goto err;
		if (json_object_array_add(jarr_settings, jobj_settings)) {
			json_object_put(jobj_settings);
			goto err;
		}
	}

	struct json_object *jarr_physes = json_object_new_array();
	if (!jarr_physes)
		goto err;
	json_object_object_add(jobj, "lsdnPhyses", jarr_physes);
	lsdn_foreach(ctx->phys_list, phys_entry, struct lsdn_phys, p) {
		struct json_object *jobj_phys = jsonify_lsdn_phys(p);
		if (!jobj_phys)
			goto err;
		if (json_object_array_add(jarr_physes, jobj_phys)) {
			json_object_put(jobj_phys);
			goto err;
		}
	}

	struct json_object *jarr_nets = json_object_new_array();
	if (!jarr_nets)
		goto err;
	json_object_object_add(jobj, "lsdnNets", jarr_nets);
	lsdn_foreach(ctx->networks_list, networks_entry, struct lsdn_net, net) {
		struct json_object *jobj_net = jsonify_lsdn_net(net);
		if (!jobj_net)
			goto err;
		if (json_object_array_add(jarr_nets, jobj_net)) {
			json_object_put(jobj_net);
			goto err;
		}
	}

	const char *str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
	if (!str)
		goto err;
	res = strdup(str);

err:
	json_object_put(jobj);
	ret_ptr(ctx, res);
}

struct dump_ctx {
	char *buf;
	size_t len;
	FILE *memstream;
	size_t indent;
	int state;
};

static struct dump_ctx *dump_ctx_create(void)
{
	struct dump_ctx *dctx = (struct dump_ctx *) malloc(sizeof(*dctx));
	if (!dctx)
		goto err;
	bzero(dctx, sizeof(*dctx));
	dctx->memstream = open_memstream(&dctx->buf, &dctx->len);
	if (!dctx->memstream)
		goto err;
	return dctx;
err:
	free(dctx);
	return NULL;
}

static inline void dump_ctx_free(struct dump_ctx *dctx)
{
	free(dctx);
}

static inline char *dump_ctx_get_buf(struct dump_ctx *dctx)
{
	fclose(dctx->memstream);
	return dctx->buf;
}

static inline int dump_ctx_check_state(struct dump_ctx *dctx)
{
	return dctx->state;
}

static inline void dump_ctx_set_state(struct dump_ctx *dctx, int state)
{
	dctx->state = state;
}

static inline void dump_ctx_inc_indent(struct dump_ctx *dctx)
{
	dctx->indent++;
}

static inline void dump_ctx_dec_indent(struct dump_ctx *dctx)
{
	assert(dctx->indent > 0);
	dctx->indent--;
}

static int dump_ctx_append_str(struct dump_ctx *dctx, const char *str)
{
	int ret = 0;
	if (dump_ctx_check_state(dctx))
		return ret;
	if ((ret = fprintf(dctx->memstream, str)) < 0)
		dump_ctx_set_state(dctx, 1);
	return ret;
}

static int dump_ctx_append(struct dump_ctx *dctx, ...)
{
	int total = 0;
	int ret;
	va_list args;
	const char *str;

	va_start(args, dctx);
	while ((str = va_arg(args, char *))) {
		if ((ret = dump_ctx_append_str(dctx, str)) < 0)
			return ret;
		else
			total += ret;
		if ((ret = dump_ctx_append_str(dctx, " ")) < 0)
			return ret;
		else
			total += ret;
	}
	va_end(args);

	return total;
}

static int dump_ctx_start_line(struct dump_ctx *dctx)
{
	int total = 0;
	int ret;
	for (size_t i = 0; i < dctx->indent; i++) {
		if ((ret = dump_ctx_append_str(dctx, "\t")) < 0)
			return ret;
		else
			total += ret;
	}
	return total;
}

static int dump_ctx_end_line(struct dump_ctx *dctx)
{
	return dump_ctx_append_str(dctx, "\n");
}

static int parse_json_lsdn_settings(struct json_object *jobj, struct dump_ctx *dctx)
{
	assert(json_object_is_type(jobj, json_type_array));
	int len = json_object_array_length(jobj);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *jsettings = json_object_array_get_idx(jobj, idx);
		assert(json_object_is_type(jsettings, json_type_object));
		dump_ctx_start_line(dctx);
		dump_ctx_append(dctx, "settings", NULL);
		struct json_object *val;
		assert(json_object_object_get_ex(jsettings, "settingsType", &val));
		dump_ctx_append(dctx, json_object_get_string(val), NULL);

		if (json_object_object_get_ex(jsettings, "settingsName", &val)) {
			dump_ctx_append(dctx, "-name", json_object_get_string(val), NULL);
		}
		if (json_object_object_get_ex(jsettings, "port", &val)) {
			dump_ctx_append(dctx, "-port", json_object_get_string(val), NULL);
		}
		if (json_object_object_get_ex(jsettings, "ip", &val)) {
			dump_ctx_append(dctx, "-mcastIp", json_object_get_string(val), NULL);
		}
		dump_ctx_end_line(dctx);
	}
	return 0;
}

static char *target_match_conversion(const char *target)
{
	static char *LOOKUP_TABLE[][2] = {
		{ "src_mac", "-srcMac" },
		{ "dst_mac", "-dstMac" },
		{ "src_ipv4", "-srcIp" },
		{ "dst_ipv4", "-dstIp" },
		{ "src_ipv6", "-srcIp" },
		{ "dst_ipv6", "-dstIp" },
		{ NULL, NULL}
	};
	char **table = *LOOKUP_TABLE;
	while (*table) {
		if (!strcmp(target, *table))
			return *++table;
		table++;
	}
	return NULL;
}

static bool target_has_mask(const char *target)
{
	static char *HAS_MASK_TABLE[] = {
		"src_ipv4",
		"dst_ipv4",
		"src_ipv6",
		"dst_ipv6",
		NULL
	};
	char **mask = HAS_MASK_TABLE;
	while (*mask) {
		if (!strcmp(target, *mask))
			return true;
		mask++;
	}
	return false;
}

static int convert_rules(struct json_object *rules, struct dump_ctx *dctx)
{
	char ip_pref[8 + 1];

	assert(json_object_is_type(rules, json_type_array));
	int len = json_object_array_length(rules);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *rule = json_object_array_get_idx(rules, idx);
		assert(json_object_is_type(rule, json_type_object));
		dump_ctx_start_line(dctx);
		dump_ctx_append(dctx, "rule", NULL);
		struct json_object *val;
		if (!json_object_object_get_ex(rule, "dir", &val))
			return 1;
		dump_ctx_append(dctx, json_object_get_string(val), NULL);
		if (!json_object_object_get_ex(rule, "prio", &val))
			return 1;
		dump_ctx_append(dctx, json_object_get_string(val), NULL);
		if (!json_object_object_get_ex(rule, "action", &val))
			return 1;
		dump_ctx_append(dctx, json_object_get_string(val), NULL);

		if (json_object_object_get_ex(rule, "targets", &val)) {
			assert(json_object_is_type(val, json_type_array));
			int targets_len = json_object_array_length(val);
			for (int idx = 0; idx < targets_len; idx++) {
				struct json_object *wal = json_object_array_get_idx(val, idx);

				struct json_object *zal;
				if (!json_object_object_get_ex(wal, "target", &zal))
					return 1;
				const char *target = json_object_get_string(zal);
				dump_ctx_append(dctx, target_match_conversion(target), NULL);
				if (!json_object_object_get_ex(wal, "match", &zal))
					return 1;
				dump_ctx_append_str(dctx, json_object_get_string(zal));
				if (target_has_mask(target)) {
					if (json_object_object_get_ex(wal, "matchMask", &zal)) {
						lsdn_ip_t ip;
						lsdn_err_t res = lsdn_parse_ip(&ip, json_object_get_string(zal));
						if (res != LSDNE_OK)
							return 1;
						int prefix = lsdn_ip_prefix_from_mask(&ip);
						sprintf(ip_pref, "/%d", prefix);
						dump_ctx_append(dctx, ip_pref, NULL);
					}
				}
			}
		}
		dump_ctx_end_line(dctx);
	}
	return 0;
}

static int convert_qos(struct json_object *qos, bool in, struct dump_ctx *dctx)
{
	assert(json_object_is_type(qos, json_type_object));
	dump_ctx_start_line(dctx);
	dump_ctx_append(dctx, "rate", NULL);
	if (in)
		dump_ctx_append(dctx, "in", NULL);
	else
		dump_ctx_append(dctx, "out", NULL);
	struct json_object *attr;
	if (json_object_object_get_ex(qos, "avgRate", &attr))
		dump_ctx_append(dctx, "-avg", json_object_get_string(attr), NULL);
	if (json_object_object_get_ex(qos, "burstSize", &attr))
		dump_ctx_append(dctx, "-burst", json_object_get_string(attr), NULL);
	if (json_object_object_get_ex(qos, "burstRate", &attr))
		dump_ctx_append(dctx, "-burstRate", json_object_get_string(attr), NULL);
	dump_ctx_end_line(dctx);
	return 0;
}

static int parse_json_lsdn_nets(struct json_object *jobj, struct dump_ctx *dctx)
{
	assert(json_object_is_type(jobj, json_type_array));
	int len = json_object_array_length(jobj);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *jnet = json_object_array_get_idx(jobj, idx);
		assert(json_object_is_type(jnet, json_type_object));
		dump_ctx_start_line(dctx);
		dump_ctx_append(dctx, "net", NULL);
		struct json_object *val;
		if (json_object_object_get_ex(jnet, "vnetId", &val))
			dump_ctx_append(dctx, "-vid", json_object_get_string(val), NULL);
		if (json_object_object_get_ex(jnet, "settings", &val))
			dump_ctx_append(dctx, "-settings", json_object_get_string(val), NULL);
		if (json_object_object_get_ex(jnet, "netName", &val))
			dump_ctx_append(dctx, json_object_get_string(val), NULL);
		dump_ctx_append(dctx, "{", NULL);
		dump_ctx_end_line(dctx);
		dump_ctx_inc_indent(dctx);

		struct json_object *phys_list;
		if (json_object_object_get_ex(jnet, "physList", &phys_list)) {
			assert(json_object_is_type(phys_list, json_type_array));
			int len_phys_list = json_object_array_length(phys_list);
			for (int idx = 0; idx < len_phys_list; idx++) {
				struct json_object *p = json_object_array_get_idx(phys_list, idx);
				dump_ctx_start_line(dctx);
				dump_ctx_append(dctx, "attach", json_object_get_string(p), NULL);
				dump_ctx_end_line(dctx);
			}
		}

		struct json_object *virt_list;
		if (json_object_object_get_ex(jnet, "virts", &virt_list)) {
			assert(json_object_is_type(virt_list, json_type_array));
			int len_virt_list = json_object_array_length(virt_list);
			for (int idx = 0; idx < len_virt_list; idx++) {
				struct json_object *p = json_object_array_get_idx(virt_list, idx);
				dump_ctx_start_line(dctx);
				dump_ctx_append(dctx, "virt", NULL);
				struct json_object *rules = NULL;
				struct json_object *qos_in = NULL;
				struct json_object *qos_out = NULL;
				json_object_object_foreach(p, pkey, pval) {
					if (!strcmp(pkey, "virtName")) {
						dump_ctx_append(dctx, "-name", json_object_get_string(pval), NULL);
					} else if (!strcmp(pkey, "phys")) {
						dump_ctx_append(dctx, "-phys", json_object_get_string(pval), NULL);
					} else if (!strcmp(pkey, "iface")) {
						dump_ctx_append(dctx, "-if", json_object_get_string(pval), NULL);
					} else if (!strcmp(pkey, "attrMac")) {
						dump_ctx_append(dctx, "-mac", json_object_get_string(pval), NULL);
					} else if (!strcmp(pkey, "qosIn")) {
						qos_in = pval;
					} else if (!strcmp(pkey, "qosOut")) {
						qos_out = pval;
					} else if (!strcmp(pkey, "rules")) {
						rules = pval;
					}
				}
				dump_ctx_append(dctx, "{", NULL);
				dump_ctx_end_line(dctx);
				dump_ctx_inc_indent(dctx);
				if (rules)
					if (convert_rules(rules, dctx))
						return 1;
				if (qos_in)
					if (convert_qos(qos_in, true, dctx))
						return 1;
				if (qos_out)
					if (convert_qos(qos_out, false, dctx))
						return 1;
				dump_ctx_dec_indent(dctx);
				dump_ctx_start_line(dctx);
				dump_ctx_append(dctx, "}", NULL);
				dump_ctx_end_line(dctx);
			}
		}
		dump_ctx_append(dctx, "}", NULL);
		dump_ctx_end_line(dctx);
		dump_ctx_dec_indent(dctx);
	}
	return 0;
}

static int parse_json_lsdn_physes(struct json_object *jobj, struct dump_ctx *dctx)
{
	assert(json_object_is_type(jobj, json_type_array));
	int len = json_object_array_length(jobj);
	for (int idx = 0; idx < len; idx++) {
		struct json_object *jphys = json_object_array_get_idx(jobj, idx);
		assert(json_object_is_type(jphys, json_type_object));
		dump_ctx_start_line(dctx);
		dump_ctx_append(dctx, "phys", NULL);
		const char *name = NULL;
		struct json_object *val;
		if (json_object_object_get_ex(jphys, "physName", &val)) {
			dump_ctx_append(dctx, "-name", NULL);
			name = json_object_get_string(val);
			dump_ctx_append(dctx, name, NULL);
		}
		if (json_object_object_get_ex(jphys, "attrIp", &val))
			dump_ctx_append(dctx, "-ip", json_object_get_string(val), NULL);
		if (json_object_object_get_ex(jphys, "iface", &val))
			dump_ctx_append(dctx, "-if", json_object_get_string(val), NULL);
		dump_ctx_end_line(dctx);
		if (json_object_object_get_ex(jphys, "isLocal", &val) && name) {
			if (json_object_get_boolean(val)) {
				dump_ctx_start_line(dctx);
				dump_ctx_append(dctx, "claimLocal", name, NULL);
				dump_ctx_end_line(dctx);
			}
		}
	}
	return 0;
}

char *lsdn_dump_context_tcl(struct lsdn_context *ctx)
{
	char *res = NULL;
	char *str_json = NULL;
	struct json_object *jobj = NULL;
	struct dump_ctx *dctx = NULL;

	str_json = lsdn_dump_context_json(ctx);
	if (!str_json)
		goto err;
	dctx = dump_ctx_create();
	if (!dctx)
		goto err;
	jobj = json_tokener_parse(str_json);
	if (!jobj)
		goto err;
	assert(json_object_is_type(jobj, json_type_object));

	dump_ctx_start_line(dctx);
	dump_ctx_append(dctx, "namespace import lsdn::*", NULL);
	dump_ctx_end_line(dctx);
	json_object_object_foreach(jobj, key, val) {
		if (!strcmp(key, "lsdnSettings")) {
			if (parse_json_lsdn_settings(val, dctx))
				goto err;
		} else if (!strcmp(key, "lsdnPhyses")) {
			if (parse_json_lsdn_physes(val, dctx))
				goto err;
		} else if (!strcmp(key, "lsdnNets")) {
			if (parse_json_lsdn_nets(val, dctx))
				goto err;
		}
	}
	if (!dump_ctx_check_state(dctx))
		res = dump_ctx_get_buf(dctx);

err:
	free(str_json);
	dump_ctx_free(dctx);
	json_object_put(jobj);

	ret_ptr(ctx, res);
}
