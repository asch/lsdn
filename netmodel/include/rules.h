#pragma once

#include "lsdn.h"
#include "nettypes.h"

#define lsdn_foreach_rule_target(x) \
	x(LSDN_MATCH_NONE, none) \
	x(LSDN_MATCH_SRC_MAC, src_mac) \
	x(LSDN_MATCH_DST_MAC, dst_mac) \
	x(LSDN_MATCH_SRC_IPV4, src_ipv4) \
	x(LSDN_MATCH_DST_IPV4, dst_ipv4) \
	x(LSDN_MATCH_SRC_IPV6, src_ipv6) \
	x(LSDN_MATCH_DST_IPV6, dst_ipv6) \
	x(LSDN_MATCH_ENC_KEY_ID, enc_key_id)

#define lsdn_rule_target_id(z, y) z,
enum lsdn_rule_target{
	lsdn_foreach_rule_target(lsdn_rule_target_id)
};
#undef lsdn_rule_target_id

const char* lsdn_rule_target_get_name(enum lsdn_rule_target t);

#define LSDN_MAX_MATCH_LEN 16
union lsdn_matchdata {
	char bytes[LSDN_MAX_MATCH_LEN];
	lsdn_mac_t mac;
	lsdn_ipv4_t ipv4;
	lsdn_ipv6_t ipv6;
	uint32_t enc_key_id;
};

#define LSDN_VR_PRIO_MIN 0
/* exclusive */
#define LSDN_VR_PRIO_MAX 0x8000

enum lsdn_direction {
	LSDN_IN,
	LSDN_OUT
};

/* lsdn virt's rule */
struct lsdn_vr;
struct lsdn_vr *lsdn_vr_new(struct lsdn_virt *virt, uint16_t prio, enum lsdn_direction dir);
struct lsdn_vr_action;

extern struct lsdn_vr_action lsdn_vr_drop;

#define lsdn_vr_shortcuts(name, type, fullmask) \
	static inline void lsdn_vr_add_##name( \
		struct lsdn_vr *rule, type value, struct lsdn_vr_action *action) \
	{ \
		lsdn_vr_add_masked_##name(rule, (fullmask), value, action); \
	} \
	static inline struct lsdn_vr *lsdn_vr_new_masked_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, uint16_t prio, \
		type mask, type value, struct lsdn_vr_action *action) \
	{ \
		struct lsdn_vr *vr = lsdn_vr_new(virt, prio, dir); \
		lsdn_vr_add_masked_##name(vr, mask, value, action); \
		return vr; \
	} \
	static inline struct lsdn_vr *lsdn_vr_new_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, \
		uint16_t prio, type value, struct lsdn_vr_action *action)  \
	{ \
		return lsdn_vr_new_masked_##name(virt, prio, dir, (fullmask), value, action); \
	}

void lsdn_vr_add_masked_src_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value, struct lsdn_vr_action *action);
lsdn_vr_shortcuts(src_mac, lsdn_mac_t, lsdn_single_mac_mask)

void lsdn_vr_add_masked_dst_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value, struct lsdn_vr_action *action);
lsdn_vr_shortcuts(dst_mac, lsdn_mac_t, lsdn_single_mac_mask)

void lsdn_vr_add_masked_src_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value, struct lsdn_vr_action *action);
lsdn_vr_shortcuts(src_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

void lsdn_vr_add_masked_dst_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value, struct lsdn_vr_action *action);
lsdn_vr_shortcuts(dst_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

#undef lsdn_vr_shortcuts
